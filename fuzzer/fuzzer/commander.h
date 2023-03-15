#ifndef COMMANDER_H
#define COMMANDER_H

#include "evolution.h"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/timer/timer.hpp>
namespace po = boost::program_options;
namespace bp = boost::process;
namespace bl = boost::lockfree;

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

#include "tbb/concurrent_hash_map.h"

#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace fuzz {

#define MAX_NUM_PROCESSES 4096

template <typename K> struct HashCompare {
  static size_t hash(const K &key) { return boost::hash_value(key); }
  static bool equal(const K &key1, const K &key2) { return (key1 == key2); }
};

typedef tbb::concurrent_hash_map<bp::process::id_type, uint64_t,
                                 HashCompare<bp::process::id_type>>
    testcase_pid_map_t;

enum ProcessStatus {
  E_PROCESS_RUNNING = 1,
  E_PROCESS_TERMINATED,
  E_PROCESS_CRASHED,
  E_PROCESS_TIMEDOUT,
  E_PROCESS_UNKNOWN = 0xff
};

void timeout_kill(const bp::process::id_type pid);
ProcessStatus get_pid_status(const bp::process::id_type pid);

struct ProcessStatuses;

// Handle timeouts
struct TimeoutWatcher {
  ProcessStatuses *processes = nullptr;

  TimeoutWatcher() = delete;
  TimeoutWatcher(const TimeoutWatcher &) = delete;
  TimeoutWatcher(ProcessStatuses *processes) : processes(processes) {}

  void operator()();
};

//
// ProcessStatuses maps the current pid with their statuses
//

typedef tbb::concurrent_hash_map<bp::process::id_type, ProcessStatus,
                                 HashCompare<bp::process::id_type>>
    pid_status_map_t;

typedef tbb::concurrent_hash_map<bp::process::id_type, boost::timer::cpu_timer,
                                 HashCompare<bp::process::id_type>>
    pid_timer_map_t;

typedef std::set<bp::process::id_type> pids_t;

struct ProcessStatuses {
  friend struct TimeoutWatcher;

  const uint32_t process_timeout_seconds;
  const boost::timer::nanosecond_type process_timeout_nanoseconds;

  std::mutex all_pids_mutex;
  pids_t all_pids;

  pid_timer_map_t pid_timers;
  pid_status_map_t pid_statuses;

  std::unique_ptr<TimeoutWatcher> watcher;
  boost::thread thread;

  ProcessStatuses(const uint32_t process_timeout_seconds);
  ProcessStatuses(const ProcessStatuses &) = delete;
  ProcessStatuses &operator=(const ProcessStatuses &) = delete;
  ~ProcessStatuses();

  bool empty() {
    std::lock_guard<std::mutex> lock(all_pids_mutex);
    return all_pids.empty();
  }

  void insert(const bp::process::id_type pid,
              const ProcessStatus status = E_PROCESS_RUNNING);

  void update(const bp::process::id_type pid,
              const ProcessStatus status = E_PROCESS_RUNNING);

  size_t num_pids() {
    std::lock_guard<std::mutex> lock(all_pids_mutex);
    return all_pids.size();
  }

  std::map<bp::process::id_type, ProcessStatus> get_terminated_processes();

  // Reset the status
  void clear();

  pids_t copy_all_pids();

private:
  void remove(const bp::process::id_type pid);
};

typedef ProcessStatuses CommanderProcesses;

// The kind of inputs supported by the fuzzer
enum CommandInputKind {
  E_COMMAND_INPUT = 1,
  E_COMMAND_FILE,
  E_COMMAND_UNKNOWN = 0xff
};

class Commander {
  struct Impl;

  const po::variables_map &vm;
  std::string command_line;
  CommandInputKind input_kind;
  Impl *impl = nullptr;

  CommanderProcesses child_processes;
  testcase_pid_map_t testcase_pids;

public:
  Commander(const po::variables_map &vm, const std::string &command_line,
            bool no_cleanup = false)
      : vm(vm), command_line(command_line),
        child_processes(vm["target-timeout-seconds"].as<uint32_t>()),
        testcase_pids(MAX_NUM_PROCESSES) {
    initialize(no_cleanup);
  }
  Commander(const Commander &) = delete;
  Commander &operator=(const Commander &) = delete;
  ~Commander();

  bool call(uint64_t testcase_id, uint8_t *data, uint32_t size);

  bool call(uint64_t testcase_id, const ga::Individual &i) {
    return call(testcase_id, i.memory.buffer(i.index),
                i.memory.length(i.index));
  }

  uint64_t get_testcase_id(const bp::process::id_type pid) const;

  CommanderProcesses &processes() { return child_processes; }

  void processed_pid(const bp::process::id_type pid);

  void clear();

  std::string get_command() const { return command_line; }

private:
  void initialize(bool no_cleanup);
  void setup_environment();
  void setup_post_processor();
  void setup_ld_preload();
};
}

#endif
