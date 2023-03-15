#include "common-instr-callbacks.h"
#include "pin.H"

//#include <boost/predef.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#if defined(BOOST_WINDOWS_API)
#error "Windows not supported"
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

PIN_LOCK lock;
ofstream log_file;

// Default values for locations...
static string instr_idir = "instr_idir/";

static string clang_runtime = "/Users/<path-to>"
                              "/libs/dist/"
                              "llvm/bin/";

static string runtime_cpp_libs =
    "/Users/<path-to>/"
    "app/dist/clang-instrument/";

static string insturment_lib_path =
    runtime_cpp_libs + "libclang-instrument.dylib";

static string runtime_lib_path =
    "/Users/<path-to>/"
    "app/dist/runtime/libinstr-runtime.a";

KNOB<string>
    KnobInstrIdir(KNOB_MODE_WRITEONCE, "pintool", "d", instr_idir,
                  "Name of the directory containing the instrumentation");
KNOB<string> KnobClangRuntime(KNOB_MODE_WRITEONCE, "pintool", "c",
                              clang_runtime, "Path to clang's runtime");
KNOB<string> KnobRuntimeCXXLibs(KNOB_MODE_WRITEONCE, "pintool", "x",
                                runtime_cpp_libs,
                                "Path to CXX libs (libc++abi, etc.)");
KNOB<string> KnobInstrLibPath(KNOB_MODE_WRITEONCE, "pintool", "i",
                              insturment_lib_path,
                              "Path to the libclang-instrument dynamic lib");
KNOB<string> KnobRuntimeStaticLibPath(
    KNOB_MODE_WRITEONCE, "pintool", "t", runtime_lib_path,
    "path to the static library containing the runtime");

int Usage() {
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

void set_globals() {
  instr_idir = KnobInstrIdir.Value();
  clang_runtime = KnobClangRuntime.Value();
  runtime_cpp_libs = KnobRuntimeCXXLibs.Value();
  insturment_lib_path = KnobInstrLibPath.Value();
  runtime_lib_path = KnobRuntimeStaticLibPath.Value();
}

void replace_all(string &str, const string &from, const string &to) {
  if (from.empty())
    return;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

struct BuildStateMachine;
BuildStateMachine *sm = nullptr;

// This state machine perform the modifications of the original command line
// and instructs what to do next i.e., perform the original call with modified
// parameters, or skip it entirely
struct BuildStateMachine {
  struct StringPosOperator {
    virtual bool does(const string &value, const string &needle) = 0;
  };

  struct EndsWith : public StringPosOperator {
    bool does(const string &value, const string &needle) {
      size_t n_pos = value.find(needle);
      if (n_pos == string::npos)
        return false;
      return n_pos == (value.length() - needle.length());
    }
  };

  struct StartsWith : public StringPosOperator {
    bool does(const string &value, const string &needle) {
      return value.find(needle) == 0;
    }
  };

  struct greater {
    template <class T> bool operator()(T const &a, T const &b) const {
      return a > b;
    }
  };

  EndsWith ends_with;
  StartsWith starts_with;

  enum ActionStatus {
    E_AS_CONTINUE = 0,
    E_AS_SKIP_ORIGINAL,
    E_AS_UNKNOWN = 0xff
  };

  enum CompilationStep { E_ST_COMPILE = 0, E_ST_LINK, E_ST_UNKNOWN = 0xff };

  pid_t *previous_pid = nullptr;
  bool had_compilation_step = false;
  CompilationStep current_step = E_ST_COMPILE;
  set<string> known_targets;
  bool enable_asan = false;

  ActionStatus instrument_spawn(CONTEXT *ctx, AFUNPTR original, pid_t *pid,
                                const char *path,
                                const posix_spawn_file_actions_t *file_actions,
                                const posix_spawnattr_t *attrp,
                                const char **argv, char *const *envp) {
    string callee = argv[0];
    vector<string> params = get_parameters(argv);
    current_step = guess_compilation_step(params);
    log_file << "  [-] guessed step: "
             << (current_step == E_ST_COMPILE ? "compilation" : "linking")
             << endl;

    if (current_step == E_ST_COMPILE)
      had_compilation_step = true;

    return current_step == E_ST_COMPILE
               ? handle_compilation_step(ctx, original, pid, path, file_actions,
                                         attrp, argv, envp, callee, params)
               : handle_linking_step(ctx, original, pid, path, file_actions,
                                     attrp, argv, envp, callee, params);
  }

private:
  ActionStatus handle_compilation_step(
      CONTEXT *ctx, AFUNPTR original, pid_t *pid, const char *path,
      const posix_spawn_file_actions_t *file_actions,
      const posix_spawnattr_t *attrp, const char **argv, char *const *envp,
      const string &callee, const vector<string> &params) {
    pair<string, string> targets = get_targets(params);

    log_file << "  [-] targets: " << targets.first << " & " << targets.second
             << endl;

    // First, create the new file
    //   clang++ -cc1 -load /path/to/libclang-instrument.dylib -plugin
    //   instrument -o /path/to/instrumented_input.cpp -fcxx-exceptions
    //   ORIGINAL_ARGS
    vector<string> new_params =
        rewrite_params_for_instr(targets, params, callee);
    call(path, callee, new_params, pid, envp);

    // Second, actually compile the generated output into the original .o
    // file
    new_params = rewrite_params_for_compilation(targets, params, callee);
    call(path, callee, new_params, pid, envp, /*no_wait*/ false);

    return E_AS_CONTINUE;
  }

  ActionStatus handle_linking_step(
      CONTEXT *ctx, AFUNPTR original, pid_t *pid, const char *path,
      const posix_spawn_file_actions_t *file_actions,
      const posix_spawnattr_t *attrp, const char **argv, char *const *envp,
      const string &callee, const vector<string> &params) {

    // For the linking, we only need to get all the orignal .o files (since we
    // didn't change them and add libstrin-runtime.a to the linker.
    pair<string, string> targets = get_targets(params);

    cout << "COVERAGE_INSTR_FINAL_BINARY=" << targets.second << endl;

    vector<string> new_params =
        rewrite_params_for_linking(targets, params, callee);
    call(path, callee, new_params, pid, envp);

    return E_AS_CONTINUE;
  }

  void call(const char *path, const string &callee,
            const vector<string> &params, pid_t *pid, char *const *environ,
            bool no_wait = false) {
    char *const *new_env =
        const_cast<char *const *>(get_new_environment(environ));

    char **argv = vector_to_argv(params);
    debug_command_line(params);

    string new_compiler = make_file(clang_runtime, callee);
    log_file << "[call] New compiler: " << new_compiler << endl;

    posix_spawn(pid, new_compiler.c_str(), nullptr, nullptr, argv, new_env);
    if (!no_wait) {
      waitpid(*pid, nullptr, 0);
    }

    // free buffers
    free_all(argv);
    free_all(new_env);
  }

  // Append new variables in the PATH
  char **get_new_environment(char *const *envp) {
    vector<string> env = get_parameters(envp);
    size_t length = env.size();

    // We need to insert a DYLD_LIBRARY_PATH, DYLD_FALLBACK_LIBRARY_PATH or
    // LD_LIBRARY_PATH if they don't exist
    size_t has_dyld_lib_path = string::npos,
           has_fallback_dylib_lib_path = string::npos,
           has_ld_lib_path = string::npos, has_path = string::npos;

    for (size_t i = 0; i < length; i++) {
      string &env_line = env[i];
      if (starts_with.does(env_line, "DYLD_LIBRARY_PATH="))
        has_dyld_lib_path = i;
      if (starts_with.does(env_line, "DYLD_FALLBACK_LIBRARY_PATH="))
        has_fallback_dylib_lib_path = i;
      if (starts_with.does(env_line, "LD_LIBRARY_PATH="))
        has_ld_lib_path = i;
      if (starts_with.does(env_line, "PATH="))
        has_path = i;
    }

    append_to_env(env, "DYLD_LIBRARY_PATH", has_dyld_lib_path,
                  runtime_cpp_libs);
    append_to_env(env, "DYLD_FALLBACK_LIBRARY_PATH",
                  has_fallback_dylib_lib_path, runtime_cpp_libs);
    append_to_env(env, "LD_LIBRARY_PATH", has_ld_lib_path, runtime_cpp_libs);

    return vector_to_argv(env);
  }

  void append_to_env(vector<string> &env, const string &key, const size_t index,
                     const string &path) {
    if (index == string::npos) {
      // Add an element to the environment
      env.push_back(key + "=" + path);
    } else {
      string &env_line = env[index];
      env_line += ((env_line.length() == key.length() + 1) ? "" : ":") + path;
    }
  }

  void debug_envion(char **environ) {
    size_t i = 0;
    while (true) {
      if (environ[i] == nullptr)
        break;
      log_file << environ[i] << endl;
      i++;
    }
  }

  void debug_command_line(const vector<string> &params) {
    ostringstream oss;
    for (auto &i : params) {
      oss << i << " ";
    }
    log_file << "  [-] new call: " << oss.str() << endl;
  }

  string make_file(const string &directory, const string &filename) {
    string tmp = directory + "/" + filename;
    replace_all(tmp, "//", "/");
    return tmp;
  }

  char **vector_to_argv(const vector<string> &params) {
    char **argv = new char *[params.size() + 1];
    for (size_t i = 0; i < params.size(); i++) {
      argv[i] = new char[params[i].length() + 1];
      strncpy(argv[i], params[i].c_str(), params[i].length());
      argv[i][params[i].length()] = '\0';
    }
    argv[params.size()] = nullptr;
    return argv;
  }

  vector<string>
  rewrite_params_for_linking(const pair<string, string> &targets,
                             const vector<string> &original_params,
                             const string &callee) {
    vector<string> new_params(original_params);
    if (enable_asan) {
      new_params.insert(new_params.begin(), "-fsanitizer=address");
    }
    new_params.insert(new_params.begin(), "-g");
    new_params.insert(new_params.begin(), callee);
    new_params.push_back(runtime_lib_path);

    for (auto &i : new_params) {
      if (known_targets.find(i) != known_targets.end()) {
        i = make_file(instr_idir, i);
      }
    }

    // If OSX, add the foundation libs:
    // -framework CoreFoundation -framework ApplicationServices -framework
    // Foundation
    new_params.push_back("-framework");
    new_params.push_back("CoreFoundation");

    new_params.push_back("-framework");
    new_params.push_back("ApplicationServices");

    new_params.push_back("-framework");
    new_params.push_back("Foundation");

    return new_params;
  }

  vector<string>
  rewrite_params_for_compilation(const pair<string, string> &targets,
                                 const vector<string> &original_params,
                                 const string &callee) {
    vector<string> new_params(original_params);

    new_params.insert(new_params.begin(), "-D_FORTIFY_SOURCE=1");
    new_params.insert(new_params.begin(), "-fstack-protector");
    if (enable_asan) {
      new_params.insert(new_params.begin(), "-fno-optimize-sibling-calls");
      new_params.insert(new_params.begin(), "-fno-omit-frame-pointer");
      new_params.insert(new_params.begin(), "-fsanitizer=address");
    }
    new_params.insert(new_params.begin(), "-g");

    // if there is a no-exception, we need to remove it
    vector<size_t> remove_for_instr;
    for (size_t i = 0; i < new_params.size(); i++) {
      if (new_params[i] == "-fno-exceptions") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-pie") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-pic") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-arch") {
        remove_for_instr.push_back(i);
        remove_for_instr.push_back(i + 1);
      }
    }
    if (remove_for_instr.size() > 0) {
      std::sort(remove_for_instr.begin(), remove_for_instr.end(), greater());
      for (auto &i : remove_for_instr) {
        new_params.erase(new_params.begin() + i);
      }
    }

    // Just rewrite the known target source into the instrumented ones.
    for (auto &i : new_params) {
      if (i == targets.first) {
        i = make_file(instr_idir, targets.first);
      } else if (i == targets.second) {
        i = make_file(instr_idir, targets.second);
      }
    }
    new_params.insert(new_params.begin(), callee);
    return new_params;
  }

  vector<string> rewrite_params_for_instr(const pair<string, string> &targets,
                                          const vector<string> &original_params,
                                          const string &callee) {
    //   clang++ -cc1 -load /path/to/libclang-instrument.dylib -plugin
    //   instrument -o /path/to/instrumented_input.cpp -fcxx-exceptions
    //   ORIGINAL_ARGS
    vector<string> new_params(original_params);
    // Push in reverse order
    new_params.insert(new_params.begin(), "instrument");
    new_params.insert(new_params.begin(), "-plugin");
    new_params.insert(new_params.begin(), insturment_lib_path);
    new_params.insert(new_params.begin(), "-load");
    new_params.insert(new_params.begin(), "-cc1");
    new_params.insert(new_params.begin(), callee);

    // if there is a no-exception, we need to remove it
    vector<size_t> remove_for_instr;
    for (size_t i = 0; i < new_params.size(); i++) {
      if (new_params[i] == "-fno-exceptions") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-c") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-g") {
        remove_for_instr.push_back(i);
      } else if (new_params[i] == "-arch") {
        remove_for_instr.push_back(i);
        remove_for_instr.push_back(i + 1);
      } else if (new_params[i] == "-o") {
        remove_for_instr.push_back(i);
        remove_for_instr.push_back(i + 1);
      }
    }
    if (remove_for_instr.size() > 0) {
      std::sort(remove_for_instr.begin(), remove_for_instr.end(), greater());
      for (auto &i : remove_for_instr) {
        new_params.erase(new_params.begin() + i);
      }
    }
    new_params.push_back("-fcxx-exceptions");
    new_params.push_back("-o");
    new_params.push_back(make_file(instr_idir, targets.first));

    return new_params;
  }

  pair<string, string> get_targets(const vector<string> &params) {
    pair<string, string> targets;
    size_t i = 0;
    for (; i < params.size(); i++) {
      if (params[i] == "-o") {
        targets.second = params[i + 1];
      } else if (ends_with.does(params[i], ".c") ||
                 ends_with.does(params[i], ".cpp") ||
                 ends_with.does(params[i], ".C")) {
        targets.first = params[i];
      }
    }
    known_targets.insert(targets.second);
    known_targets.insert(targets.first);
    return targets;
  }

  CompilationStep guess_compilation_step(const vector<string> &params) {
    size_t compile_clues_nums = 0, link_clues_num = 0;
    const string compile_clues[] = {"-c",    "-std=*", "-stdlib=*",
                                    "*.cpp", "*.c",    "*.C",
                                    "-I*",   "-D*",    "-O*"},
                 link_clues[] = {"-l*", "-L*"};
    compile_clues_nums = find_clues(params, compile_clues, 9);
    link_clues_num = find_clues(params, link_clues, 2);
    return link_clues_num > 0 ? E_ST_LINK : E_ST_COMPILE;
  }

  size_t find_clues(const vector<string> &params, const string clues[],
                    const size_t num_clues) {
    const size_t num_arr_elmts = params.size();
    size_t res = 0;

    for (size_t i = 0; i < num_clues; i++) {
      const string &c = clues[i];
      const size_t star_pos = c.find('*');
      if (star_pos == string::npos) {
        if (find(params.begin(), params.end(), c) != params.end()) {
          res++;
        }
      } else {
        // Either startswith/endswith based on the position of the position of
        // the star
        string rw_clue = star_pos < 1 ? c.substr(1) : c.substr(0, star_pos);
        StringPosOperator *op =
            star_pos < 1 ? dynamic_cast<StringPosOperator *>(&ends_with)
                         : dynamic_cast<StringPosOperator *>(&starts_with);
        for (size_t j = 0; j < num_arr_elmts; j++) {
          if (op->does(params[j], rw_clue))
            res++;
        }
      }
    }

    return res;
  }

  template <typename T> void free_all(T arr) {
    if (arr == nullptr)
      return;
    size_t i = 1;
    while (true) {
      if (arr[i] == nullptr)
        break;
      delete[] arr[i];
      i++;
    }
    delete[] arr;
  }

  template <typename T> vector<string> get_parameters(T argv) {
    vector<string> params;
    size_t i = 1;
    while (true) {
      if (!argv[i])
        break;
      params.push_back(string(argv[i]));
      i++;
    }
    return params;
  }
};

void Fini(INT32 code, VOID *v) {
  if (sm != nullptr)
    delete sm;
  log_file << "ends" << endl;
  log_file.close();
}

void handler(int sig) { log_file << "Signal: " << sig << endl; }

string print_command_line(const char *path, const char **argv) {
  ostringstream oss;
  oss << path;
  size_t i = 1;
  while (true) {
    if (!argv[i])
      break;
    oss << " " << argv[i];
    i++;
  }
  return oss.str();
}

bool should_intercept_command(const char *path, const char **argv) {
  return strstr(path, "/clang") != nullptr;
}

/*
int posix_spawn(pid_t *restrict pid,
                const char *restrict path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *restrict attrp,
                char *const argv[restrict],
                char *const envp[restrict]);
*/
int wrap_posix_spawn(THREADID tid, CONTEXT *ctx, AFUNPTR original, pid_t *pid,
                     const char *path,
                     const posix_spawn_file_actions_t *file_actions,
                     const posix_spawnattr_t *attrp, const char **argv,
                     char *const *envp) {
  int ret = 0;

  CALL_APPLICATION_FUNCTION_PARAM param;
  memset((void *)&param, 0x00, sizeof(param));

  log_file << "Wrapped call to posix_spawn: " << print_command_line(path, argv)
           << endl;

  if (should_intercept_command(path, argv)) {
    log_file << " [+] Should intercept call..." << endl;
    BuildStateMachine::ActionStatus post_instr_status = sm->instrument_spawn(
        ctx, AFUNPTR(original), pid, path, file_actions, attrp, argv, envp);

    // Do we want to skip the original call?
    if (post_instr_status == BuildStateMachine::E_AS_SKIP_ORIGINAL)
      return ret;
  }

  // clang-format off
  PIN_CallApplicationFunction(ctx,
                              PIN_ThreadId(),
                              CALLINGSTD_DEFAULT,
                              AFUNPTR(original),
                              &param,
                                PIN_PARG(int), &ret,
                                PIN_PARG(pid_t *), pid,
                                PIN_PARG(const char *), path,
                                PIN_PARG(const posix_spawn_file_actions_t *), file_actions,
                                PIN_PARG(const posix_spawnattr_t *), attrp,
                                PIN_PARG(const char **), argv,
                                PIN_PARG(char *const *), envp,
                              PIN_PARG_END());
  // clang-format on
  sm->previous_pid = pid;

  log_file << "Got ret: " << ret << " pid=" << std::hex << (unsigned long)pid
           << endl;

  return ret;
}

void iproto_jitted_posix_spawn(RTN routine, const string &rtnName) {
  // clang-format off
  PROTO proto = START_PROTO(int)
                  PIN_PARG(pid_t *),
                  PIN_PARG(const char *),
                  PIN_PARG(const posix_spawn_file_actions_t *),
                  PIN_PARG(const posix_spawnattr_t *),
                  PIN_PARG(const char **),
                  PIN_PARG(char *const *),
                END_PROTO;

  START_REPLACE_JITTED(wrap_posix_spawn, proto)
    REPLACE_INSERT_ARG(0),
    REPLACE_INSERT_ARG(1),
    REPLACE_INSERT_ARG(2),
    REPLACE_INSERT_ARG(3),
    REPLACE_INSERT_ARG(4),
    REPLACE_INSERT_ARG(5),
  END_REPLACE;

  CLEAR_PROTO(proto);
  // clang-format on
}

void print_all_routines(IMG img, VOID *v) {
  if (!IMG_Valid(img))
    return;

  RTN rtn = RTN_FindByName(img, "_posix_spawn");
  if (RTN_Valid(rtn)) {
    log_file << "Found _posix_spawn" << endl;
    iproto_jitted_posix_spawn(rtn, "posix_spawn");
  }
}

void print_routine(RTN rtn, VOID *v) {
  // log_file << "routine=" << RTN_Name(rtn) << endl;
}

BOOL follow_child(CHILD_PROCESS childProcess, VOID *userData) { return true; }

// VOID instr_routine(RTN rtn, VOID *v) { const string &rtn_name =
// RTN_Name(rtn); }

int main(int argc, char *argv[]) {
  PIN_InitSymbols();
  if (PIN_Init(argc, argv)) {
    cerr << "Init failed." << endl;
    Usage();
    return 0;
  }

  set_globals();
  PIN_InitLock(&lock);

  sm = new BuildStateMachine();
  log_file.open("custom-pin-instr-spaw.log",
                std::ofstream::out | std::ofstream::trunc);
  log_file << "Start pintool" << endl;
  log_file.flush();

  // notify when following child process
  PIN_AddFollowChildProcessFunction(follow_child, 0);
  IMG_AddInstrumentFunction(print_all_routines, 0);
  PIN_AddFiniFunction(Fini, 0);

  // OSX only supports Jitted mode
  PIN_StartProgram();

  return 0;
}
