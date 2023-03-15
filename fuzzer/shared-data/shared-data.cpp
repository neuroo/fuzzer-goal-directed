#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "shared-data.h"
#include <boost/lexical_cast.hpp>
using namespace boost::interprocess;
using namespace std;

namespace shm {

#define NASTY_DEBUG 0

static const std::string SHARED_MEMORY_NAME =
    "5bac7840-9158-43c7-98d1-889369e4f0f0";

static const char SHARED_TRACES_MUTEX_NAME[] = "__CTM_889369e4f0f0";

static const std::size_t SHARED_MEMORY_SIZE = 5 * 104857600; // 500MB

const char *traceKindName(const TraceKind kind) {
  switch (kind) {
  case E_TRUE_BRANCH:
    return "E_TRUE_BRANCH";
  case E_FALSE_BRANCH:
    return "E_FALSE_BRANCH";
  case E_ENTER_FUNCTION:
    return "E_ENTER_FUNCTION";
  case E_EXIT_FUNCTION:
    return "E_EXIT_FUNCTION";
  case E_EXCEPTION_BRANCH:
    return "E_EXCEPTION_BRANCH";
  case E_KILL:
    return "E_KILL";
  case E_TERMINATED:
    return "E_TERMINATED";
  case E_CRASHED:
    return "E_CRASHED";
  case E_TIMEDOUT:
    return "E_TIMEDOUT";
  default:
    return "E_UNKNOWN";
  }
}

std::string TraceElement::toString() const {
  ostringstream oss;
  oss << "<TraceElement kind=" << traceKindName(kind)
      << " thread_id=" << thread_id << " func_id=" << func_id;
  if (pred_block_id > 0) {
    oss << " " << pred_block_id << "->" << cur_block_id;
  }
  oss << ">";
  return oss.str();
}

bool operator==(const TraceElement &a, const TraceElement &b) {
  return a.kind == b.kind && a.thread_id == b.thread_id &&
         a.func_id == b.func_id && a.pred_block_id == b.pred_block_id &&
         a.cur_block_id == b.cur_block_id;
}

//
// Container related methods
//

Container::Container(managed_shared_memory *shm, segment_manager_t *seg_mgr)
    : shm(shm), inner_trace_alloc(seg_mgr) {
  traces_mutex = std::unique_ptr<shared_data_mutex_t>(
      new shared_data_mutex_t(open_or_create, SHARED_TRACES_MUTEX_NAME));
}

Container::~Container() {
  //
}

void Container::add(const key_type testcase_id,
                    const element_value_type trace_element) {
  boost::interprocess::scoped_lock<shared_data_mutex_t> lock(
      *traces_mutex.get());
  trace_t *trace = get_create_list(testcase_id);
  trace->push_back(trace_element);
}

void Container::add(const key_type testcase_id,
                    const std::list<element_value_type> &received_trace) {
  boost::interprocess::scoped_lock<shared_data_mutex_t> lock(
      *traces_mutex.get());
  trace_t *trace = get_create_list(testcase_id);
  for (auto &i : received_trace) {
    trace->push_back(i);
  }
}

std::string Container::get_trace_name(const key_type testcase_id) {
  return std::to_string(testcase_id);
}

Container::trace_t *Container::get_trace(const key_type testcase_id) {
  boost::interprocess::scoped_lock<shared_data_mutex_t> lock(
      *traces_mutex.get());
  const std::string name = get_trace_name(testcase_id);
  return shm->find<trace_t>(name.c_str()).first;
}

void Container::remove_trace(const key_type testcase_id) {
  boost::interprocess::scoped_lock<shared_data_mutex_t> lock(
      *traces_mutex.get());
  const std::string name = get_trace_name(testcase_id);
  const bool success = shm->destroy<trace_t>(name.c_str());
#if (NASTY_DEBUG == 1)
  std::cout << "remove trace: " << testcase_id << " success=" << success
            << std::endl;
#endif
}

Container::trace_t *Container::get_create_list(const key_type testcase_id) {
  const std::string name = get_trace_name(testcase_id);
  return shm->find_or_construct<trace_t>(name.c_str())(inner_trace_alloc);
}

std::string Container::toString() {
  using namespace std;
  return "<Container>";
}

//
// Runtime client and Fuzzer client to the shared memory
//
void SHMRuntimeWriter::create_shm() {
  bool not_exists = true;
  // This is a pretty ghetto way to know the shared memory has been prepared.
  // Shouldn't be necessary now that the client can also create the shared
  // memory, still here for now.
  bool open_only_flag = true;
  unsigned int iterations = 0;
  while (not_exists) {
    sleep(1);
    ++iterations;
    try_create_shm();
    not_exists = !segment;
  }

  try {
    if (segment) {
      container = new Container(segment, segment->get_segment_manager());
    }
  } catch (const std::exception &ex) {
#if (NASTY_DEBUG == 1)
    std::cerr << "Exception- " << ex.what() << std::endl;
#endif
    if (segment)
      delete segment;
    if (container)
      delete container;
    segment = nullptr;
    container = nullptr;
    create_shm();
  }
}

void SHMRuntimeWriter::try_create_shm() {
  if (segment)
    return;

  if (container) {
    delete container;
    container = nullptr;
  }
  try {
    segment = new managed_shared_memory(
        open_or_create, SHARED_MEMORY_NAME.c_str(), SHARED_MEMORY_SIZE);
  } catch (std::exception &ex) {
#if (NASTY_DEBUG == 1)
    std::cerr << "Exception- " << ex.what() << std::endl;
#endif
    delete segment;
    segment = nullptr;
  }
}

bool SHMRuntimeWriter::exists(bool open_only_flag) {
  managed_shared_memory *s = nullptr;
  try {
    s = new managed_shared_memory(open_only, SHARED_MEMORY_NAME.c_str());
    bool value = s->check_sanity();
    if (s)
      delete s;
    return value;
  } catch (const std::exception &ex) {
//
#if (NASTY_DEBUG == 1)
    std::cerr << "Exception- " << ex.what() << std::endl;
#endif
  }
  if (s)
    delete s;
  return false;
}

SHMFuzzerHandler::SHMFuzzerHandler(bool cleanup, bool remove_mutexes)
    : cleanup(cleanup) {
  if (remove_mutexes) {
    named_mutex::remove(SHARED_TRACES_MUTEX_NAME);
  }

  if (cleanup) {
#if (NASTY_DEBUG == 1)
    std::cout << "SHMFuzzerHandler- cleaning existing SHM data" << std::endl;
#endif
    shared_memory_object::remove(SHARED_MEMORY_NAME.c_str());
  }
  segment = new managed_shared_memory(
      open_or_create, SHARED_MEMORY_NAME.c_str(), SHARED_MEMORY_SIZE);
  bool value = segment->check_sanity();
  if (!value) {
#if (NASTY_DEBUG == 1)
    std::cout << "SHMFuzzerHandler- Unsane SHM" << std::endl;
#endif
  }
  container = new Container(segment, segment->get_segment_manager());
}

size_t SHMFuzzerHandler::red_free_size() {
  const size_t current_size = segment->get_size();
  const size_t free_size = segment->get_free_memory();
  return free_size < (current_size >> 2);
}

void SHMFuzzerHandler::force_remove_mutexes() {
  named_mutex::remove(SHARED_TRACES_MUTEX_NAME);
}

// Less than 25% free, we grow
void SHMFuzzerHandler::update_size() {
#if (NASTY_DEBUG == 1)
  std::cout << "grow size of segment, current size=" << current_size
            << std::endl;
#endif
  delete segment;
  delete container;
  named_mutex::remove(SHARED_TRACES_MUTEX_NAME);
  managed_shared_memory::grow(SHARED_MEMORY_NAME.c_str(), SHARED_MEMORY_SIZE);

  segment = new managed_shared_memory(open_only, SHARED_MEMORY_NAME.c_str());
  container = new Container(segment, segment->get_segment_manager());
}

bool SHMFuzzerHandler::is_sane() { return segment && segment->check_sanity(); }

size_t SHMFuzzerHandler::free_size() { return segment->get_free_memory(); }

SHMFuzzerHandler::~SHMFuzzerHandler() {
  if (cleanup) {
    shared_memory_object::remove(SHARED_MEMORY_NAME.c_str());
  }
  if (container) {
    delete container;
  }
  if (segment) {
    delete segment;
  }
}
}
