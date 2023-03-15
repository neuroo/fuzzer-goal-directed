#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

// Comment out to use the direct object shared memory
//#define USE_FILE_BACKED_MEMORY
//
//#ifdef USE_FILE_BACKED_MEMORY
//  #pragma error
//  #include <boost/interprocess/file_mapping.hpp>
//#endif

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/list.hpp>
#include <boost/interprocess/containers/map.hpp>

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/upgradable_lock.hpp>

namespace shm {
namespace ipc = boost::interprocess;

// The kind of elements that are shared from the runtime to the fuzzer.
enum TraceKind {
  E_TRUE_BRANCH = 0,
  E_FALSE_BRANCH = 1,
  E_ENTER_FUNCTION = 2,
  E_EXIT_FUNCTION = 3,
  E_EXCEPTION_BRANCH = 4,
  E_KILL = 5,
  E_TERMINATED = 6,
  E_CRASHED = 7,
  E_TIMEDOUT = 8,
  E_UNKNOWN = 0xff
};

const char *traceKindName(const TraceKind kind);

// This is the object that captures the runtime information that's
// coming from the SUT into the shared memory. This represents a simple
// program point, so it is unique. That allows us to limit the
struct TraceElement {
  TraceKind kind = E_UNKNOWN;
  uint64_t thread_id = 0;
  uint32_t func_id = 0;
  uint32_t pred_block_id = 0; // no polymorphism, we can just add these
  uint32_t cur_block_id = 0;  // two integers all the time

  TraceElement() = default;
  ~TraceElement() = default;
  TraceElement(const TraceKind kind, const uint64_t thread_id = 0,
               const uint32_t func_id = 0, const uint32_t pred_block_id = 0,
               const uint32_t cur_block_id = 0)
      : kind(kind), thread_id(thread_id), func_id(func_id),
        pred_block_id(pred_block_id), cur_block_id(cur_block_id) {}

  TraceElement(const TraceElement &e)
      : kind(e.kind), thread_id(e.thread_id), func_id(e.func_id),
        pred_block_id(e.pred_block_id), cur_block_id(e.cur_block_id) {}

  TraceElement &operator=(const TraceElement &e) {
    if (&e != this) {
      kind = e.kind;
      thread_id = e.thread_id;
      func_id = e.func_id;
      pred_block_id = e.pred_block_id;
      cur_block_id = e.cur_block_id;
    }
    return *this;
  }

  std::string toString() const;
};

bool operator==(const TraceElement &a, const TraceElement &b);

//
// Interprocess code
//

typedef ipc::managed_shared_memory::segment_manager segment_manager_t;

// The container represents the structure in shared memory. The runtime writes
// to it and the fuzzer reads from it (for now). The data that's captured is
// the following:
//  - `statuses`: the association of a testcase id and an `ExecutionStatus`
//  - `trace_\d+`: an std::list of `TraceElement` used as a stack.
//                 This is essentially the raw data from the runtime.
struct Container {
  typedef ipc::interprocess_upgradable_mutex upgradable_mutex_type;
  typedef ipc::named_mutex named_mutex;
  typedef named_mutex shared_data_mutex_t;

  typedef unsigned long key_type; // testcase id

  typedef TraceElement element_value_type;

  // Definition of a list of `element_value_type` living in the SHM
  typedef ipc::allocator<element_value_type, segment_manager_t>
      shm_trace_alloc_t;
  typedef ipc::list<element_value_type, shm_trace_alloc_t> trace_t;
  typedef std::list<element_value_type> mocked_trace_t;
  typedef mocked_trace_t heap_trace_t;

  // The allocators are managed by the container itself, but the shared
  // memory pointer is provided by a `Writer` or `Reader`
  ipc::managed_shared_memory *shm = nullptr;

  std::unique_ptr<shared_data_mutex_t> traces_mutex;

  shm_trace_alloc_t inner_trace_alloc;

  Container() = delete;
  Container(const Container &) = delete;
  Container &operator=(const Container &) = delete;
  ~Container();

  Container(ipc::managed_shared_memory *shm, segment_manager_t *seg_mgr);

  // Add an element in its trace. If the trace doesn't exist, it gets created
  // and its status is set to E_ACTIVE
  void add(const key_type testcase_id, const element_value_type trace_element);

  void add(const key_type testcase_id,
           const std::list<element_value_type> &trace);

  std::string get_trace_name(const key_type testcase_id);

  std::string toString();

  trace_t *get_trace(const key_type testcase_id);

  void remove_trace(const key_type testcase_id);

private:
  // Get or create the list associated to the testcase_id. This API cannot be
  // called
  // directly as it's not protected by a lock.
  trace_t *get_create_list(const key_type testcase_id);

  // A handler that check the current size of the SHM and grows it if we're
  // approaching full size
  void update_size();
};

// The writer is embedded in the runtime, so it can only create the SHM or
// write
// to it, it doesn't remove it when the program terminates (since it's still
// used by the fuzzer).
struct SHMRuntimeWriter {
  ipc::managed_shared_memory *segment = nullptr;
  Container *container = nullptr;

  SHMRuntimeWriter() { create_shm(); }

  SHMRuntimeWriter(const SHMRuntimeWriter &) = delete;
  SHMRuntimeWriter &operator=(const SHMRuntimeWriter &) = delete;

  ~SHMRuntimeWriter() {
    if (container) {
      delete container;
    }
    if (segment) {
      delete segment;
    }
  }

private:
  void create_shm();

  void try_create_shm();

  bool exists(bool open_only_flag = true);
};

// The fuzzer is mostly the one to create the SHM and remove it. If there is
// an
// existing SHM with the same name (might not be ours), it will be deleted
struct SHMFuzzerHandler {
  bool cleanup = true;
  ipc::managed_shared_memory *segment = nullptr;
  Container *container = nullptr;

  SHMFuzzerHandler(const SHMFuzzerHandler &) = delete;

  SHMFuzzerHandler(bool cleanup = false, bool remove_mutexes = true);

  void update_size();
  void force_remove_mutexes();

  size_t red_free_size();
  bool is_sane();
  size_t free_size();

  ~SHMFuzzerHandler();
};
}

#endif
