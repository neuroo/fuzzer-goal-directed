#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

#include <boost/predef.h>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
namespace bp = boost::process;
namespace fs = boost::filesystem;

#if (BOOST_OS_MACOS || BOOST_OS_LINUX)
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif BOOST_OS_WINDOWS
#error "I don't have a windows machine"
#else
#error "Unsupported platform"
#endif

#include "shared-data/shared-data.h"
using namespace shm;

#define STREAM_OUTPUT_PARENT 1
#define NUMBER_USABLE_FORKS 50

bp::child start_process(const string &executable,
                        const vector<string> &process_args,
                        const bp::posix_context &ctx) {
  return bp::posix_launch(executable, process_args, ctx);
}

enum Status {
  E_PROCESS_RUNNING = 1,
  E_PROCESS_TERMINATED,
  E_PROCESS_CRASHED,
  E_PROCESS_UNKNOWN = 0xff
};

Status get_pid_status(const pid_t pid) {
  int status;
  pid_t w = waitpid(pid, &status, 0);
  if (w < 0) {
    perror("waitpid");
    return E_PROCESS_UNKNOWN;
  }

  if (WIFEXITED(status)) {
    return E_PROCESS_TERMINATED;
  }
  return E_PROCESS_RUNNING;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <path/to/prog/to/start>" << endl;
    return 1;
  }

  std::string executable = argv[1];
  fs::path executable_path(executable);
  if (!fs::is_regular_file(executable_path)) {
    cerr << "Argument is not a regular process" << endl;
    return 1;
  }
  bp::posix_context ctx;
  ctx.environment = bp::self::get_environment();

#if (STREAM_OUTPUT_PARENT == 1)
  ctx.output_behavior.insert(
      bp::behavior_map::value_type(STDOUT_FILENO, bp::inherit_stream()));
  ctx.output_behavior.insert(
      bp::behavior_map::value_type(STDERR_FILENO, bp::inherit_stream()));
#endif

  vector<string> args;
  args.push_back(executable);

  bp::child child = start_process(executable, args, ctx);
  pid_t pid = child.get_id();
  cout << "Child PID=" << pid << endl;

  while (true) {
    const Status s = get_pid_status(pid);
    if (s == E_PROCESS_RUNNING) {
      continue;
    } else {
      break;
    }
  }

  return 0;
}
