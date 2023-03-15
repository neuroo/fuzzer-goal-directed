#!/usr/bin/env bash
rm -f fuzzing.log
ASAN_OPTIONS="symbolize=1:verbosity=1" ASAN_SYMBOLIZER_PATH=/Users/<path-to>/libs/dist/llvm/bin/llvm-symbolizer DYLD_INSERT_LIBRARIES=/Users/<path-to>/libs/dist/llvm/lib/clang/3.8.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib ./coverage-fuzz --fake-target-call=true -- t
