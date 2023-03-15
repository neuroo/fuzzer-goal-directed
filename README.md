# feedback-goal-directed-fuzzing
Some stuff about a feedback-directed, goal-directed fuzzing architecture.


## Installation
 1. Go to `./libs` and run all `install-*.sh` scripts
 2. Go to `./fuzzer` and run `make`

The last step will create all components, from the instrumentation plugin to the runtime that's injected in the binary to test.


## Architecture
There are several components in play to enable a fuzzer:
 1. `clang-instrument`: source-to-source instrumentation engine and goal extraction
 2. `shared-data`: library to share information between SUT and the fuzzer during its execution
 3. `runtime`: contains hooks that are injected in SUT during compile-time

### `clang-instrument`
The instrumentation technology is based on LLVM/clang. The binary is actually a plugin for clang 3.8 so little insertions in the invocation of clang needs to be made for generating the instrumented source code. Assuming `ORIGNAL_CLANG_ARGS`, the instrumentation and compilation steps are:

```
instrumentation:
  clang++ -cc1 -load /path/to/libclang-instrument.dylib -plugin instrument
          ORIGNAL_CLANG_ARGS -o /path/to/instrumented_input.cpp
          -fcxx-exceptions
compilation:
  clang++ /path/to/instrumented_input.cpp ORIGNAL_CLANG_ARGS libinstr-runtime.a
          -o ORIGINAL_CU_TARGET
```

This will generate a `model.xxx` file that contains all information related to the compiled source including goals, functions, blocks, etc.

#### Source-to-source Instrumentation
The instrumentation encodes little information in the source itself, so for the following input function (dummied for example purposes):
```c++
int is_b_next(const char *c) {
  const char cc = *(c + 1);
  if (cc == 'B') {
    return 1;
  }
  else if (cc == 'C')
    return -1;
  return 0;
}
```

The instrumented code is the following (prettified):
```c++
int is_b_next(const char *c) {
  int __coverage_ret_value;
  unsigned int __coverage_pred_block = 0;
  __coverage_enter_func(/*func_id*/42);

  const char cc = *(c + 1);
  if (cc == 'B') {
    __coverage_reach_block(/*func_id*/42, __coverage_pred_block, 1);
    __coverage_pred_block = 1;

    __coverage_ret_value = (1);
    __coverage_exit_func(/*func_id*/42);
    return __coverage_ret_value;
  }
  else if (cc == 'C') {
    __coverage_reach_block(/*func_id*/42, __coverage_pred_block, 2);
    __coverage_pred_block = 2;

    __coverage_ret_value = (-1);
    __coverage_exit_func(/*func_id*/42);
    return __coverage_ret_value;
  }

  __coverage_ret_value = (0);
  __coverage_exit_func(/*func_id*/42);
  return __coverage_ret_value;
}
```

Some information about the instrumentation:
- We record edges in the CFG and not just the basic block ID each time. That's helpful within loops
- We know exactly when a function returns to create a precise trace in the fuzzer-land
- Some code is changed such as the else-stmt with no braces

