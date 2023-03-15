#ifndef FUZZER_H
#define FUZZER_H

#include "common/store.h"
#include "shared-data/shared-data.h"

#include "commander.h"
#include "crash-analyzer.h"
#include "evolution.h"
#include "knowledge.h"
#include "measure.h"
#include "memory-manager.h"
#include "mocker.h"
#include "seeds.h"
#include "utils.h"

#include "tbb/concurrent_queue.h"

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
namespace bl = boost::lockfree;

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace fuzz {

namespace ui {
struct WebSocketServer; // fwd declaration
struct UICrashReader;   // fwd declaration
}

struct FuzzerMonitor;
struct FuzzerDriver;
struct FuzzerStats;
struct FuzzerProcessMonitor;
struct FuzzerTraceRetriever;
struct FuzzerCrashAnalyzer;
struct FuzzerUI;
struct FuzzerUIQueueReader;

// typedef bl::spsc_queue<uint64_t, bl::capacity<65536>> testcase_queue_t;
typedef tbb::concurrent_queue<uint64_t> testcase_queue_t;

class FuzzerHandler {
  friend struct FuzzerMonitor;
  friend struct FuzzerDriver;
  friend struct FuzzerStats;
  friend struct FuzzerProcessMonitor;
  friend struct FuzzerTraceRetriever;
  friend struct FuzzerCrashAnalyzer;
  friend struct FuzzerUI;
  friend struct FuzzerUIQueueReader;
  friend struct ui::WebSocketServer;

  // Args & process exec
  const po::variables_map &vm;
  bool skip_calling_target = false;
  bool disable_ui = false;
  std::string command_line;

  MemoryManager memory;

  std::unique_ptr<shm::SHMFuzzerHandler> shm_handler;
  std::unique_ptr<Commander> commander;
  std::unique_ptr<Mocker> mocker;

  // Current status
  uint64_t max_num_testcases;
  std::atomic_ullong current_testcase_id;
  std::atomic_ullong generated_testcases;
  utils::timer_t timer;

  std::unique_ptr<SeedHandler> seeds;
  std::unique_ptr<FuzzerDriver> driver;

  FuzzerUI *handler_ui = nullptr;

public:
  FuzzerHandler() = delete;
  FuzzerHandler(const FuzzerHandler &) = delete;
  FuzzerHandler &operator=(const FuzzerHandler &) = delete;
  ~FuzzerHandler() = default;

  FuzzerHandler(const po::variables_map &vm) : vm(vm) {}

  void start();
  void set_command_line(const std::string &cmd_line) {
    command_line = cmd_line;
  }

  shm::SHMRuntimeWriter isolated_shm_handler;
  std::atomic_ulong number_isolated_testcases;
  uint64_t isolated_testcases[500] = {0};
  measure::trace_score_t isolated_evaluation(const std::string &input_buffer);

  bool is_isolated_testcase(const uint64_t testcase_id) const;

private:
  bool has_more_cases();
  void initialize();
  void init_seeds(const uint32_t remaining_places,
                  const uint32_t population_deviation);

  void reset_shared_memory(FuzzerTraceRetriever *trace_retriever);
  void clear_queue(testcase_queue_t &queue);

  void multi_threaded_loop(const uint32_t num_threads);
  void single_threaded_loop();
};

// A simple monitor to print out stuff
struct FuzzerMonitor {
  FuzzerHandler &fuzzer_handler;

  FuzzerMonitor() = delete;
  FuzzerMonitor &operator=(const FuzzerMonitor &) = delete;

  FuzzerMonitor(FuzzerHandler &fuzzer_handler)
      : fuzzer_handler(fuzzer_handler) {}

  void operator()();
};

// A simple monitor to print out stuff
struct FuzzerStats {
  FuzzerHandler &fuzzer_handler;

  FuzzerStats() = delete;
  FuzzerStats &operator=(const FuzzerStats &) = delete;

  FuzzerStats(FuzzerHandler &fuzzer_handler) : fuzzer_handler(fuzzer_handler) {}

  void operator()();

  void compute_overall_fitness();
};

// This drives the algorithm used to generate the population
struct FuzzerDriver {
  std::unique_ptr<utils::Rand> random;
  MemoryManager &memory;

  // This is the GA implementation. It takes a reference to the population
  // and manages the mutations/cross-overs
  std::unique_ptr<ga::Evolver> evolver;
  std::unique_ptr<ga::Population> population;
  std::unique_ptr<ProgramKnowledge> knowledge;

  FuzzerDriver() = delete;
  FuzzerDriver(MemoryManager &memory) : memory(memory) {}
  FuzzerDriver(const FuzzerDriver &) = delete;
  FuzzerDriver &operator=(const FuzzerDriver &) = delete;

  void init_population();
  void one_generation(bool skip_evolution = false);
};

struct FuzzerProcessMonitor {
  FuzzerHandler &fuzzer_handler;
  testcase_queue_t &queue;
  testcase_queue_t &timed_out_queue;
  std::set<uint64_t> all_processed;

  std::atomic_bool all_testscases_received;
  std::atomic_bool all_testscases_sent;

  FuzzerProcessMonitor() = delete;
  FuzzerProcessMonitor &operator=(const FuzzerProcessMonitor &) = delete;

  FuzzerProcessMonitor(FuzzerHandler &fuzzer_handler, testcase_queue_t &queue,
                       testcase_queue_t &timed_out_queue)
      : fuzzer_handler(fuzzer_handler), queue(queue),
        timed_out_queue(timed_out_queue) {}

  void operator()();

  void clear();
};

struct FuzzerTraceRetriever {
  FuzzerHandler &fuzzer_handler;
  std::unique_ptr<shm::SHMRuntimeWriter> shm_handler;

  testcase_queue_t &queue;
  testcase_queue_t &timed_out_queue;
  std::atomic_ulong processed_testcases;

  std::set<uint64_t> all_received;
  std::set<uint64_t> all_processed;
  std::atomic_bool all_testscases_received;
  std::atomic_bool all_testscases_sent;

  FuzzerTraceRetriever() = delete;
  FuzzerTraceRetriever &operator=(const FuzzerTraceRetriever &) = delete;

  FuzzerTraceRetriever(FuzzerHandler &fuzzer_handler, testcase_queue_t &queue,
                       testcase_queue_t &timed_out_queue)
      : fuzzer_handler(fuzzer_handler), queue(queue),
        timed_out_queue(timed_out_queue) {
    processed_testcases.store(0);
    shm_handler =
        std::unique_ptr<shm::SHMRuntimeWriter>(new shm::SHMRuntimeWriter());
  }

  void operator()();

  bool process(const uint64_t testcase_id);

  void clear();
};

struct FuzzerCrashAnalyzer {
  FuzzerHandler &fuzzer_handler;
  CrashAnalyzer crash_analyzer;

  FuzzerCrashAnalyzer() = delete;
  FuzzerCrashAnalyzer &operator=(const FuzzerCrashAnalyzer &) = delete;

  FuzzerCrashAnalyzer(FuzzerHandler &fuzzer_handler, const std::string &idir,
                      const std::string &sdir)
      : fuzzer_handler(fuzzer_handler), crash_analyzer(idir, sdir) {}

  void operator()();
};

//
// FuzzerUI
//
typedef bl::spsc_queue<ga::ShareableIndividual, bl::capacity<65536>>
    individual_queue_t;
typedef bl::spsc_queue<coverage_t, bl::capacity<65536>> coverage_queue_t;
typedef bl::spsc_queue<std::set<uint64_t>, bl::capacity<65536>>
    crashing_queue_t;

struct FuzzerUI {
  FuzzerHandler &fuzzer_handler;
  std::unique_ptr<ui::WebSocketServer> server;
  individual_queue_t testcase_queue;
  coverage_queue_t coverage_queue;
  crashing_queue_t crashing_queue;
  const bool disable_ui;

  FuzzerUI() = delete;
  FuzzerUI &operator=(const FuzzerUI &) = delete;

  FuzzerUI(FuzzerHandler &fuzzer_handler, const uint32_t port,
           const std::string &docroot_str, const std::string &data_store_str,
           const bool disable_ui = false);

  void operator()();

  void push_testcase(const ga::ShareableIndividual &ind);
  void
  consume_local_coverage(const std::map<instr::element_id, uint32_t> coverage);

  void set_crashing_testcase_ids(const std::set<uint64_t> &testcase_ids);
};

struct FuzzerUIQueueReader {
  FuzzerUI &fuzzer_ui;

  FuzzerUIQueueReader() = delete;
  FuzzerUIQueueReader &operator=(const FuzzerUIQueueReader &) = delete;

  FuzzerUIQueueReader(FuzzerUI &fuzzer_ui) : fuzzer_ui(fuzzer_ui) {}

  void operator()();
};

struct FuzzerUICrashReader {
  FuzzerUI &fuzzer_ui;
  ui::UICrashReader *crash_reader = nullptr;

  FuzzerUICrashReader() = delete;
  FuzzerUICrashReader &operator=(const FuzzerUICrashReader &) = delete;

  FuzzerUICrashReader(FuzzerUI &fuzzer_ui, ui::UICrashReader *crash_reader)
      : fuzzer_ui(fuzzer_ui), crash_reader(crash_reader) {}

  void operator()();
};
}

#endif
