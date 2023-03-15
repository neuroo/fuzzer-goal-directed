#include "commander.h"
#include "common/logger.h"
#include "utils.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/filesystem.hpp>
#include <boost/predef.h>
#include <boost/process.hpp>
#include <boost/thread/thread.hpp>
#include <boost/tokenizer.hpp>
namespace bp = boost::process;
namespace fs = boost::filesystem;
namespace ba = boost::algorithm;
namespace bt = boost::timer;

#if defined(BOOST_WINDOWS_API)
#include <windows.h>
#else
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// TBB
using namespace tbb;

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

namespace fuzz {

static const std::string FUZZING_CRASH_ME = "COVERAGE_FUZZING_CRASH_ME";
static const std::string ENV_TESTCASE_ID = "COVERAGE_FUZZING_TESTCASE_ID";
static const std::string INPUT_NEEDLE = "__INPUT__";
static const std::string FILE_NEEDLE = "__FILE__";

//
// Some process utils
//
void timeout_kill(const bp::process::id_type pid) {
#if (BOOST_OS_MACOS || BOOST_OS_LINUX)
  kill(pid, SIGUSR2); // notify the child of the timeout. it will then flush the
                      // trace and then commit suicide
#else
#error "Windows not supported yet for processes."
#endif
}

void kill_them_all(const bp::process::id_type pid) {
#if (BOOST_OS_MACOS || BOOST_OS_LINUX)
  kill(-pid, SIGKILL);
  kill(pid, SIGKILL);
  int status;
  waitpid(pid, &status, 0);
#else
#error "Windows not supported yet for processes."
#endif
}

ProcessStatus get_pid_status(const bp::process::id_type pid) {
#if (BOOST_OS_MACOS || BOOST_OS_LINUX)
  int status;
  pid_t w = waitpid(pid, &status, WNOHANG | WUNTRACED);
  if (w < 0) {
    if (errno == EINTR) {
      return get_pid_status(pid);
    }
    return E_PROCESS_RUNNING;
  } else if (w == 0) {
    return E_PROCESS_RUNNING;
  } else {
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      return E_PROCESS_TERMINATED;
    }
  }

  return E_PROCESS_UNKNOWN;
#else
#error "Windows not supported yet for processes."
  return E_PROCESS_UNKNOWN;
#endif
}

void set_group_child_process(const bp::process::id_type pid) {
#if (BOOST_OS_MACOS || BOOST_OS_LINUX)
  if (setpgid(pid, pid) == -1) {
    LOG(ERROR) << "Cannot daemonize this process";
  }
#else
  return E_PROCESS_UNKNOWN;
#endif
}

//
// ProcessStatuses::TimeoutWatcher
//
void TimeoutWatcher::operator()() {
  LOG(INFO) << "TimeoutWatcher started.";
  LOG(INFO) << " processes->process_timeout_nanoseconds="
            << processes->process_timeout_nanoseconds;

  while (true) {
    if (processes->empty()) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      continue;
    }

    boost::this_thread::sleep(boost::posix_time::milliseconds(10));

    pids_t pids = processes->copy_all_pids();
    for (auto &pid : pids) {
      auto status = get_pid_status(pid);
      switch (status) {
      case E_PROCESS_RUNNING: {
        pid_timer_map_t::const_accessor acc;
        if (processes->pid_timers.find(acc, pid)) {
          const bt::cpu_times elapsed_times(acc->second.elapsed());
          boost::timer::nanosecond_type elapsed(elapsed_times.system +
                                                elapsed_times.user);
          if (elapsed > processes->process_timeout_nanoseconds) {
            timeout_kill(pid);
            processes->update(pid, E_PROCESS_TIMEDOUT);
          }
        }
        break;
      }
      default: {
        processes->update(pid, status);
        break;
      }
      }
    }
  }
}

//
// ProcessStatuses
//
ProcessStatuses::ProcessStatuses(const uint32_t process_timeout_seconds)
    : process_timeout_seconds(process_timeout_seconds),
      process_timeout_nanoseconds(process_timeout_seconds * 1000000000LL) {
  watcher = std::unique_ptr<TimeoutWatcher>(new TimeoutWatcher(this));
  thread = boost::thread(boost::ref(*(watcher.get())));
}

ProcessStatuses::~ProcessStatuses() {
  thread.join();

  if (!all_pids.empty()) {
    for (auto &pid : all_pids) {
      kill_them_all(pid);
    }
  }
}

void ProcessStatuses::insert(const bp::process::id_type pid,
                             const ProcessStatus status) {
  {
    pid_status_map_t::accessor acc;
    if (pid_statuses.insert(acc, pid)) {
      acc->second = status;
    }
  }

  if (status == E_PROCESS_RUNNING) {
    pid_timer_map_t::accessor acc;
    if (pid_timers.insert(acc, pid)) {
      acc->second = boost::timer::cpu_timer();
    }
  }

  {
    std::lock_guard<std::mutex> lock(all_pids_mutex);
    all_pids.insert(pid);
  }
}

void ProcessStatuses::update(const bp::process::id_type pid,
                             const ProcessStatus status) {
  pid_status_map_t::accessor acc;
  if (pid_statuses.find(acc, pid)) {
    acc->second = status;
  }
}

// private. only called by this process manager
void ProcessStatuses::remove(const bp::process::id_type pid) {
  {
    pid_status_map_t::accessor acc;
    if (!pid_statuses.find(acc, pid))
      return;
    pid_statuses.erase(acc);
  }

  {
    pid_timer_map_t::accessor acc;
    if (pid_timers.find(acc, pid)) {
      pid_timers.erase(acc);
    }
  }

  {
    std::lock_guard<std::mutex> lock(all_pids_mutex);
    all_pids.erase(pid);
  }
}

std::map<bp::process::id_type, ProcessStatus>
ProcessStatuses::get_terminated_processes() {
  std::map<bp::process::id_type, ProcessStatus> current_terminated;
  pids_t copy_pids = copy_all_pids();
  pids_t to_remove;

  pid_status_map_t::const_accessor acc;
  for (auto &pid : copy_pids) {
    if (pid_statuses.find(acc, pid)) {
      if (acc->second != E_PROCESS_RUNNING) {
        current_terminated.insert({pid, acc->second});
        to_remove.insert(pid);
        // kill_them_all(pid);
      }
    }
    acc.release();
  }

  // Remove these pids from what's available
  for (auto &pid : to_remove) {
    remove(pid);
  }

  return current_terminated;
}

pids_t ProcessStatuses::copy_all_pids() {
  std::lock_guard<std::mutex> lock(all_pids_mutex);
  pids_t copy_pids;
  std::copy(all_pids.begin(), all_pids.end(),
            std::inserter(copy_pids, copy_pids.begin()));
  return copy_pids;
}

void ProcessStatuses::clear() {
  for (auto &pid : copy_all_pids()) {
    remove(pid);
  }

  {
    std::lock_guard<std::mutex> lock(all_pids_mutex);
    all_pids.clear();
  }
}

//
// Commander::Impl
//
struct Commander::Impl {
  map<string, string> env;
  set<string> ld_preload;

  string command_line;
  fs::path idir;
  CommandInputKind input_kind;

  string executable;
  vector<string> args;
  uint32_t input_index;
  bool force_crash_target = false;

#if defined(BOOST_POSIX_API)
  bp::posix_context ctx;
#else
  bp::win32_context ctx;
#endif

  Impl() {}
  Impl(const string &command_line, const CommandInputKind input_kind,
       const po::variables_map &vm, bool no_cleanup)
      : command_line(command_line), input_kind(input_kind) {
    initialize(vm, no_cleanup);
  }

  bp::process::id_type exec(uint64_t testcase_id, uint8_t *data, uint32_t size);
  bp::child __internal_exec(const vector<string> &process_args);

private:
  void initialize(const po::variables_map &vm, bool no_cleanup = false);
  void parse_command_line();
  void parse_extra_env(const string &env_options);
  std::string replace_fuzz_input(const std::string &input, uint64_t testcase_id,
                                 uint8_t *data, uint32_t size);
  std::string to_input(uint64_t testcase_id, uint8_t *data, uint32_t size);
  std::string to_string_input(uint8_t *data, uint32_t size);
  std::string to_file_input(uint64_t testcase_id, uint8_t *data, uint32_t size);
};

void Commander::Impl::initialize(const po::variables_map &vm, bool no_cleanup) {
// Set the original environment from shell that runs coverage-fuzzer
// ctx.environment = bp::self::get_environment();

#if defined(BOOST_POSIX_API)
  if (vm["stream-target-stdout"].as<bool>()) {
    ctx.output_behavior.insert(
        bp::behavior_map::value_type(STDOUT_FILENO, bp::inherit_stream()));
    ctx.output_behavior.insert(
        bp::behavior_map::value_type(STDERR_FILENO, bp::inherit_stream()));
  }
#endif

  const string env_options =
      vm.count("environment") ? vm["environment"].as<string>() : "";
  if (!env_options.empty()) {
    parse_extra_env(env_options);
  }

  force_crash_target = vm["force-crash-target"].as<bool>();

  // Setup the intermediate directory, create it if it doesn't exist
  idir = fs::path(vm["idir"].as<string>());
  if (!fs::is_directory(idir)) {
    try {
      fs::create_directories(idir / "dumps");
      fs::create_directories(idir / "crashes");
      fs::create_directories(idir / "results");
      fs::create_directories(idir / "sockets");
    } catch (exception &e) {
      LOG(ERROR) << "Cannot create idir in " << idir
                 << " - Exception: " << e.what();
      exit(0);
    }
  } else {
    // clear the directory
    if (no_cleanup == false) {
      if (vm.count("clear-idir") && vm["clear-idir"].as<bool>()) {
        try {
          fs::remove_all(idir);
          fs::create_directories(idir);
        } catch (exception &e) {
          LOG(ERROR) << "Could not clean the idir"
                     << " - Exception: " << e.what();
        }
      }
    }
  }

  // Parse the command line to set the executable and args
  parse_command_line();

  if (!env.empty()) {
    for (auto &env_name_value : env) {
      ctx.environment.insert(bp::environment::value_type(
          env_name_value.first, env_name_value.second));
    }
  }

  if (force_crash_target) {
    ctx.environment.insert(bp::environment::value_type(FUZZING_CRASH_ME, "1"));
  }
}

// Split by ; then by =, and trim
void Commander::Impl::parse_extra_env(const string &env_options) {
  boost::char_separator<char> sep_semi_colon(";");
  boost::char_separator<char> sep_equal("=");

  boost::tokenizer<boost::char_separator<char>> tokens(env_options,
                                                       sep_semi_colon);
  for (auto &env_line : tokens) {
    if (!env_line.empty() && env_line.find("=") != string::npos) {
      boost::tokenizer<boost::char_separator<char>> env_name_value(env_line,
                                                                   sep_equal);
      vector<string> values;
      for (auto &x : env_name_value) {
        values.push_back(x);
      }

      if (values.size() != 2) {
        LOG(ERROR) << "Wrong format of environment line: \"" << env_line
                   << "\". Should be name=value";
        continue;
      }

      boost::trim_all(values[0]);
      boost::trim_all(values[1]);
      string &env_name = values[0];
      if (env.find(env_name) != env.end()) {
        LOG(ERROR) << "Cannot have duplicate environment variables for "
                   << env_name;
        continue;
      }
      env.insert({values[0], values[1]});
    }
  }

  LOG(INFO) << "Using extra env variables:\n" << env;
}

size_t first_non_escaped_whitespace(const string &input) {
  const char *str = input.c_str();
  const size_t length = input.size();

  // XXX handle quotes
  for (size_t i = 0; i < length; i++) {
    const char c = str[i];
    if (c == ' ' && (i > 0 && str[i - 1] != '\\'))
      return i;
  }
  return string::npos;
}

// Serialize the data and escape it for the shell
std::string Commander::Impl::to_string_input(uint8_t *data, uint32_t size) {
  std::string input;
  input.assign(data, data + size * sizeof(uint8_t));
  return input;
}

// Write the data to a file and return the path to this file
std::string Commander::Impl::to_file_input(uint64_t testcase_id, uint8_t *data,
                                           uint32_t size) {
  ostringstream testcase_filename;
  testcase_filename << "tc_" << testcase_id;
  const string filename = (idir / testcase_filename.str()).string();

  ofstream file_contents(filename, ios::out | ios::binary);
  file_contents.write(reinterpret_cast<char *>(data), size * sizeof(uint8_t));
  file_contents.close();

  return (idir / filename).string();
}

//@deprecated
std::string Commander::Impl::to_input(uint64_t testcase_id, uint8_t *data,
                                      uint32_t size) {
  switch (input_kind) {
  case E_COMMAND_INPUT:
    return to_string_input(data, size);
  case E_COMMAND_FILE:
    return to_file_input(testcase_id, data, size);
  default:
    break;
  }
  return "";
}

void Commander::Impl::parse_command_line() {
  // XXX that's not strong enough when there are spaces in the path to the
  //     binary. Need to search for first non-escaped space.
  const size_t first_space = first_non_escaped_whitespace(command_line);
  executable = command_line.substr(0, first_space);

  args.clear();
  args.push_back(executable);

  // Get the full path to the binary...
  fs::path executable_path(executable);
  if (!fs::is_regular_file(executable_path)) {
    try {
      executable = bp::find_executable_in_path(executable);
    } catch (...) {
      LOG(ERROR) << "Cannot find the executable as specified (directly or in "
                    "the path). Please use its absolute path.";
      exit(0);
    }
  }

  string remaining = command_line.substr(first_space + 1);

  boost::char_separator<char> sep(" ");
  boost::tokenizer<boost::char_separator<char>> tokens(remaining, sep);

  uint32_t i = 1;
  for (auto &cmd_elmt : tokens) {
    if (cmd_elmt.find(INPUT_NEEDLE) != string::npos ||
        cmd_elmt.find(FILE_NEEDLE) != string::npos) {
      input_index = i;
    }
    args.push_back(cmd_elmt);
  }

  LOG(INFO) << "Executable: " << executable;
  LOG(INFO) << "  args: " << args;
}

std::string Commander::Impl::replace_fuzz_input(const std::string &input,
                                                uint64_t testcase_id,
                                                uint8_t *data, uint32_t size) {
  string new_input(input);
  if (input_kind == E_COMMAND_INPUT) {
    ba::replace_all(new_input, INPUT_NEEDLE, to_string_input(data, size));
  } else {
    ba::replace_all(new_input, FILE_NEEDLE,
                    to_file_input(testcase_id, data, size));
  }
  return new_input;
}

bp::process::id_type Commander::Impl::exec(uint64_t testcase_id, uint8_t *data,
                                           uint32_t size) {
  ctx.environment.erase(ENV_TESTCASE_ID);
  ctx.environment.insert(
      bp::environment::value_type(ENV_TESTCASE_ID, to_string(testcase_id)));

  vector<string> process_args;
  std::copy(args.begin(), args.end(), std::back_inserter(process_args));
  process_args[input_index] =
      replace_fuzz_input(process_args[input_index], testcase_id, data, size);

  try {
    bp::child child = __internal_exec(process_args);
    const bp::process::id_type pid = child.get_id();
    // set_group_child_process(pid);
    return pid;
  } catch (exception &e) {
    LOG(ERROR) << "Exception: " << e.what();
    return -1;
  }
}

bp::child Commander::Impl::__internal_exec(const vector<string> &process_args) {
#if defined(BOOST_POSIX_API)
  return bp::posix_launch(executable, process_args, ctx);
#else
  STARTUPINFO si;
  ::ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE;
  si.dwX = 0;
  si.dwY = 0;
  si.dwXSize = 640;
  si.dwYSize = 480;

  ctx.startupinfo = &si;
  return bp::win32_launch(executable, process_args, ctx);
#endif
}

void Commander::initialize(bool no_cleanup) {
  if (command_line.empty())
    return;

  const bool is_inlined_input =
      command_line.find(INPUT_NEEDLE) != std::string::npos;
  const bool is_file_input =
      command_line.find(FILE_NEEDLE) != std::string::npos;

  if (is_inlined_input && is_file_input) {
    LOG(ERROR) << "Multiple inputs. Not supported yet.";
    return;
  }

  input_kind = is_inlined_input
                   ? E_COMMAND_INPUT
                   : (is_file_input ? E_COMMAND_FILE : E_COMMAND_UNKNOWN);
  impl = new Impl(command_line, input_kind, vm, no_cleanup);

  setup_environment();
  setup_post_processor();
  setup_ld_preload();
}

void Commander::setup_environment() { return; }

void Commander::setup_post_processor() { return; }

void Commander::setup_ld_preload() { return; }

bool Commander::call(uint64_t testcase_id, uint8_t *data, uint32_t size) {
  if (impl) {
    try {
      const auto pid = impl->exec(testcase_id, data, size);
      if (pid < 0) {
        LOG(ERROR) << "Process call for testcase_id=" << testcase_id
                   << " failed";
        return false;
      }
      LOG(INFO) << "Assign pid=" << pid << " with testcase_id=" << testcase_id;

      {
        testcase_pid_map_t::accessor acc;
        if (testcase_pids.insert(acc, pid)) {
          acc->second = testcase_id;
        }
      }

      child_processes.insert(pid);
      return true;
    } catch (exception &e) {
      LOG(ERROR) << "Exception: " << e.what();
    }
  }
  return false;
}

uint64_t Commander::get_testcase_id(const bp::process::id_type pid) const {
  testcase_pid_map_t::const_accessor acc;
  if (!testcase_pids.find(acc, pid))
    return 0;
  return acc->second;
}

void Commander::processed_pid(const bp::process::id_type pid) {
  testcase_pid_map_t::accessor acc;
  if (!testcase_pids.find(acc, pid))
    return;
  testcase_pids.erase(acc);
}

void Commander::clear() { child_processes.clear(); }

Commander::~Commander() {
  if (impl)
    delete impl;
}
}
