#ifndef RUNTIME_H
#define RUNTIME_H

#if (__cplusplus >= 201103L)
#define CPP_11_SUPPORT
#endif

size_t get_thread_id();

// For function f. made the transition from BBL 1 to BBL 2
void __coverage_reach_block(const unsigned long, const unsigned int,
                       const unsigned int);

// For function f. did not make transition from BBL 1 to BBL 2
void __coverage_skip_block(const unsigned long, const unsigned int,
                      const unsigned int);

void __coverage_enter_func(const unsigned long);
void __coverage_exit_func(const unsigned long);

void __coverage_kill(const unsigned long);

void __coverage_terminated();

void __coverage_install_atexit();

unsigned long __coverage_get_testcase_id();

#include "shared-data/shared-data.h"
#include <atomic>
#include <list>
#include <mutex>
#include <vector>

namespace runtime {

// A singleton guard that's thread safe (using double-lock mechanism). It's used
// to capture the only reference
class SHMRuntimeWriterSingleton {
public:
  static SHMRuntimeWriterSingleton *Instance();

  ~SHMRuntimeWriterSingleton() {
    if (ptee) {
      delete ptee;
    }
  }

  inline shm::SHMRuntimeWriter *get() { return ptee; }

  // We don't write in real-time inside the shared memory, we just flush once
  // every N trace elements.
  void flush();

  void add(const shm::TraceElement &trace_element);

private:
  SHMRuntimeWriterSingleton() { initialize(); }

  void initialize();

  static std::atomic<SHMRuntimeWriterSingleton *> instance;
  static std::mutex mutex;
  static shm::SHMRuntimeWriter *ptee;
  static std::list<shm::TraceElement> trace;
  static std::mutex trace_mutex;
};

void install_breakpad();
void install_signal_handler();
}

#endif
