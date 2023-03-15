#include <atomic>
#include <cstdlib>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include "runtime.h"
using namespace shm;

#include <boost/predef.h>
#if BOOST_OS_WINDOWS
#error "This has never been tested in Windows platforms..."
#elif BOOST_OS_LINUX
#include <client/linux/handler/exception_handler.h>
#include <google_breakpad/processor/minidump.h>
#include <google_breakpad/processor/minidump_processor.h>

#include <cerrno>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif BOOST_OS_MACOS
#include <client/mac/handler/exception_handler.h>
#include <google_breakpad/processor/minidump.h>
#include <google_breakpad/processor/minidump_processor.h>

#include <cerrno>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// XXX Need to actually do something cross-platform to get the current thread
#ifdef CPP_11_SUPPORT
#include <thread>
size_t get_thread_id() {
  return (size_t)std::hash<std::thread::id>()(std::this_thread::get_id());
}
#else
#include <pthread.h>
size_t get_thread_id() {
  pthread_id_np_t tid = pthread_getthreadid_np();
  return (size_t)tid;
}
#endif

#define NASTY_DEBUG 1
#define NO_BREAKPAD 0

#define DEFAULT_TESTCASE_ID 0
#define FLUSH_EVENT_RATE 100

static std::atomic_bool sharedMemoryInitialized(false);
static std::atomic_bool progTerminationHandlerInstalled(false);
static std::atomic_bool testCaseIdFetched(false);
static std::atomic_ulong testCaseId(DEFAULT_TESTCASE_ID);

static std::atomic_ulong eventsSinceLastFlush(0);

//
// Internal runtime stuff. Uses the shared memory to communicate.
//

namespace runtime {
typedef std::unique_ptr<google_breakpad::ExceptionHandler>
    p_exception_handler_t;

static p_exception_handler_t bp_exceptionHandler = nullptr;

std::atomic<SHMRuntimeWriterSingleton *> SHMRuntimeWriterSingleton::instance{
    nullptr};
std::mutex SHMRuntimeWriterSingleton::mutex;
std::mutex SHMRuntimeWriterSingleton::trace_mutex;
shm::SHMRuntimeWriter *SHMRuntimeWriterSingleton::ptee = nullptr;
std::list<TraceElement> SHMRuntimeWriterSingleton::trace;

static SHMRuntimeWriterSingleton *writer = nullptr;

// instantiate the singleton
SHMRuntimeWriterSingleton *SHMRuntimeWriterSingleton::Instance() {
  if (instance == nullptr) {
    std::lock_guard<std::mutex> lock(mutex);
    if (instance == nullptr) {
      instance = new SHMRuntimeWriterSingleton();
    }
  }
  return instance;
}

void handle_trace_element(const TraceElement &trace_element) {
  runtime::writer->add(trace_element);

  if (eventsSinceLastFlush >= FLUSH_EVENT_RATE) {
    runtime::writer->flush();
    eventsSinceLastFlush = 0;
  }
}

void SHMRuntimeWriterSingleton::initialize() {
  // XXX create another mode where we dump it all in a normal file?
  ptee = new shm::SHMRuntimeWriter();
  // Install the atexit & breakpad hooks when this singleton gets constructed.
  __coverage_install_atexit();
}

void SHMRuntimeWriterSingleton::flush() {
  std::lock_guard<std::mutex> lock(trace_mutex);
  if (trace.empty())
    return;

#if (NASTY_DEBUG == 1)
  std::cout << "Flushing current data" << std::endl;
#endif

  size_t num = 0;
  const unsigned long tc_id = __coverage_get_testcase_id();
  get()->container->add(tc_id, trace);
  trace.clear();

#if (NASTY_DEBUG == 1)
  std::cout << "Dumped trace for tc_id=" << tc_id << std::endl;
#endif
}

void SHMRuntimeWriterSingleton::add(const TraceElement &trace_element) {
  std::lock_guard<std::mutex> lock(trace_mutex);
  trace.push_back(trace_element);
}

static bool breakpad_dump_callback(const char *dump_dir,
                                   const char *minidump_id, void *context,
                                   bool succeeded) {
#if (NASTY_DEBUG == 1)
  std::cout << "breakpad_dump_callback for " << __coverage_get_testcase_id()
            << std::endl;
#endif

  runtime::handle_trace_element(TraceElement(E_CRASHED));
  runtime::writer->flush();

#if (NASTY_DEBUG == 1)
  std::cout << "breakpad_dump_callback for " << __coverage_get_testcase_id()
            << " succeeded=" << succeeded << std::endl;
#endif
  return succeeded;
}

static void handle_custom_signal(int signum) {
#if BOOST_OS_LINUX || BOOST_OS_MACOS
#if (NASTY_DEBUG == 1)
  std::cout << "handle_custom_signal for " << __coverage_get_testcase_id()
            << std::endl;
#endif
  runtime::handle_trace_element(TraceElement(E_TIMEDOUT));
  runtime::writer->flush();
#if (NASTY_DEBUG == 1)
  std::cout << "handle_custom_signal exits " << __coverage_get_testcase_id()
            << std::endl;
#endif
  std::exit(0);
#else
#pragma error "This has never been tested in Windows platforms..."
#endif
}

void install_signal_handler() {
#if (NASTY_DEBUG == 1)
  std::cout << "install_signal_handler for " << __coverage_get_testcase_id()
            << " pid = " << getpid() << std::endl;
#endif

#if BOOST_OS_LINUX || BOOST_OS_MACOS
  signal(SIGUSR2, handle_custom_signal);
#else
#pragma error "This has never been tested in Windows platforms..."
#endif
}

void install_breakpad() {
#if (NO_BREAKPAD == 1)
  return;
#endif

  if (bp_exceptionHandler)
    return;

#if (NASTY_DEBUG == 1)
  std::cout << "[[COVERAGE_INSTR_RUNTIME]] Install breakpad" << std::endl;
#endif
  const std::string dump_out_dir = ".fuzz-idir/dumps/";

  int status =
      mkdir(dump_out_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

#if BOOST_OS_LINUX
  bp_exceptionHandler =
      p_exception_handler_t(new google_breakpad::ExceptionHandler(
          dump_out_dir,           /* minidump output directory */
          0,                      /* filter */
          breakpad_dump_callback, /* minidump callback */
          0,                      /* callback_context */
          true                    /* install_handler */
          ));

#elif BOOST_OS_MACOS
  bp_exceptionHandler =
      p_exception_handler_t(new google_breakpad::ExceptionHandler(
          dump_out_dir,           /* minidump output directory */
          0,                      /* filter */
          breakpad_dump_callback, /* minidump callback */
          0,                      /* callback_context */
          true,                   /* install_handler */
          0 /* port name, set to null so in-process dump generation is used. */
          ));

#elif BOOST_OS_WINDOWS
#pragma error "This has never been tested in Windows platforms..."
#else
#pragma error "Unsupported platform?!"
#endif
}
}

//
// Code called by the instrumentation. This has to be minimal and do call into
// interaction::* helper functions in order to create a stack of path trace
// information. When the program exits, the trace is sent to the shared memory.
//
void __coverage_reach_block(const unsigned long func_id,
                       const unsigned int pred_block_id,
                       const unsigned int cur_block_id) {
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " true_branch(" << pred_block_id << "->"
            << cur_block_id << ")" << std::endl;
#endif
  runtime::handle_trace_element(TraceElement(
      E_TRUE_BRANCH, get_thread_id(), func_id, pred_block_id, cur_block_id));
}

void __coverage_skip_block(const unsigned long func_id,
                      const unsigned int pred_block_id,
                      const unsigned int cur_block_id) {
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " false_branch(" << pred_block_id << "->"
            << cur_block_id << ")" << std::endl;
#endif
  runtime::handle_trace_element(TraceElement(
      E_FALSE_BRANCH, get_thread_id(), func_id, pred_block_id, cur_block_id));
}

void __coverage_enter_func(const unsigned long func_id) {
  if (!sharedMemoryInitialized) {
    runtime::writer = runtime::SHMRuntimeWriterSingleton::Instance();
    sharedMemoryInitialized = true;
  }
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " enter_func(" << func_id << ")" << std::endl;
#endif
  runtime::handle_trace_element(
      TraceElement(E_ENTER_FUNCTION, get_thread_id(), func_id));
}

void __coverage_exit_func(const unsigned long func_id) {
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " exit_func(" << func_id << ")" << std::endl;
#endif
  runtime::handle_trace_element(
      TraceElement(E_EXIT_FUNCTION, get_thread_id(), func_id));
}

void __coverage_kill(const unsigned long func_id) {
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " kill_fuzzing(" << func_id << ")"
            << std::endl;
#endif
  runtime::handle_trace_element(TraceElement(E_KILL, get_thread_id(), func_id));
}

void __coverage_terminated() {
  if (!sharedMemoryInitialized) {
    runtime::writer = runtime::SHMRuntimeWriterSingleton::Instance();
    sharedMemoryInitialized = true;
  }
#if (NASTY_DEBUG == 1)
  std::cout << get_thread_id() << " exit_program()" << std::endl;
#endif
  runtime::handle_trace_element(TraceElement(E_TERMINATED));
  runtime::writer->flush();
}

void __coverage_install_atexit() {
#if (NASTY_DEBUG == 1)
  std::cout << "[[RUNTIME]] Installed" << std::endl;
#endif
  if (!progTerminationHandlerInstalled) {
    progTerminationHandlerInstalled = true;
    std::atexit(__coverage_terminated);

    // Install a signal handler to flush the trace when there is a
    // timeout sent from the fuzzer
    runtime::install_signal_handler();

    // We installed a normal termination handler with `atexit`, that's used to
    // notify the execution of a given testcase was clean. Now, however, we need
    // to handle the most important cases, where a program might crash.
    // For this, we use breakpad to handle the crash and report a minidump that
    // the fuzzer can inspect to get deeper insight.
    runtime::install_breakpad();

    // After breakpad is installed, we can inject our fault
    if (const char *crash_test = std::getenv("COVERAGE_FUZZING_CRASH_ME")) {
      const unsigned int crash_test_num =
          std::strtoull(crash_test, nullptr, 10);
      if (crash_test_num == 1) {
        int *woot_ptr = reinterpret_cast<int *>(runtime::handle_trace_element);
        *woot_ptr = 0xDEADBEEF;
      }
    }
  }
}

unsigned long __coverage_get_testcase_id() {
  if (!testCaseIdFetched) {
    // The fuzzer needs to create a special env when it will set the testcase id
    if (const char *tc_id = std::getenv("COVERAGE_FUZZING_TESTCASE_ID")) {
      const unsigned long tc_id_num = std::strtoull(tc_id, nullptr, 10);
#if (NASTY_DEBUG == 1)
      std::cout << "read testcase ID: " << tc_id_num << std::endl;
#endif
      testCaseId.store(tc_id_num);
      testCaseIdFetched = true;
    }
  }
  return testCaseId.load();
}
