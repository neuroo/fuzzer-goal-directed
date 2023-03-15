#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace std;

#include "common/logger.h"
INITIALIZE_EASYLOGGINGPP; // Just once at the root

#include "handler.h"
#include "utils.h"
using namespace fuzz;

static const std::string DEFAULT_IDIR = ".fuzz-idir";
static const unsigned int DEFAULT_CONCURRENCY = 1;
static const unsigned long long DEFAULT_MAX_NUM_TESTCASES = -1; // unbounded
static const uint32_t DEFAULT_POPULATION_SIZE = 300;
static const uint32_t DEFAULT_POPULATION_DEVIATION = 20;
static const uint32_t DEFAULT_BUFFER_SIZE = 8;      // bytes
static const uint32_t DEFAULT_BUFFER_DEVIATION = 2; // bytes
static const uint32_t DEFAULT_MAX_NUM_PROCESSES = 350;
static const uint32_t DEFAULT_PROCESS_TIMEOUT_SECONDS = 30;   // 30s
static const uint32_t DEFAULT_EVOLUTION_TIMEOUT_SECONDS = 60; // 30s
static const uint32_t DEFAULT_UI_PORT = 8987;
static const uint32_t DEFAULT_MAX_EVOLUTION_FIXPOINT = 250;

// Ideally our fuzzer can work in multiple modes. The basic (and only currently)
// developed is the standalone mode where the fuzzer works on a single binary
// as a typical fuzzer. Would be nice to have:
//  - Streaming: for long running processes or servers, have a way
//               for the fuzzer to call into that process using some API
//  - Master/Slave: to enable distributed fuzzing. The master would direct all
//                  other fuzzer on a network and send them info about what
//                  to fuzz. This implies we can partition the knowledge.
enum FuzzerMode {
  E_MODE_STANDALONE = 1,
  E_MODE_STREAMING,
  E_MODE_SLAVE,
  E_MODE_MASTER,
  E_MODE_UNKNOWN = 0xff
};

//
// Input passed to the fuzzer. A few ways to specify what needs to be fuzzed:
// - "--call" with a string argument.
// - " -- ... "
// - "--script" which will be executed (shell script, python, etc.)
//
// For any way, we understand 2 ways to communicate our fuzzed input to the SUT:
//  - __FILE__ : path to a file containing the fuzzed data
//  - __INPUT__: the actual fuzzed data (mostly for string data)
// when using __INPUT__, the data might need to be processed (e.g., escaped)
// when it's part of a command line argument since binary data can live there.
//

// If -- exists split the current commmand line and keep everything before
// in `new_argv` and everything after in `command_line`.
int extract_command_line(int argc, char *argv[], string &command_line) {
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--")) {
      ostringstream oss;
      for (int j = i + 1; j < argc; j++) {
        oss << argv[j] << " ";
      }
      command_line = oss.str();
      return i;
    }
  }
  return argc;
}

int main(int argc, char *argv[]) {
  try {
    string command_line;
    const int new_argc = extract_command_line(argc, argv, command_line);

    cout << "Command line:" << command_line << endl;

    po::options_description all_opts(
        "Prototype feedback, goal directed fuzzer");

    // clang-format off
    po::options_description general_options("General options");
    general_options.add_options()
      ("help,h", "produce this help message")
      // When the SUT has side effects, do not use concurrency unless we can find some
      // kind of a VM to wrap it in.
      ("concurrency,c", po::value<uint32_t>()->default_value(DEFAULT_CONCURRENCY), "number of concurrent runs")
      ("debug,v", po::value<bool>()->default_value(false), "debugging information")
      ("idir,d", po::value<string>()->default_value(DEFAULT_IDIR), "path to intermediate directory")
      ("clear-idir", po::value<bool>()->default_value(true), "remove all files previously set in the idir")
      ("call", po::value<string>()->default_value(""), "command line to execute the target with input specifiers")
      ("script", po::value<string>()->default_value(""), "shell script to execute the target with input specifiers");

    po::options_description fuzzing_options("Fuzzing options");
    fuzzing_options.add_options()
      ("rand-seed", po::value<uint32_t>()->default_value(0), "seed for the random generator")
      ("population-initial-size", po::value<uint32_t>()->default_value(DEFAULT_POPULATION_SIZE), "initial size of the population to use")
      ("population-deviation-size", po::value<uint32_t>()->default_value(DEFAULT_POPULATION_DEVIATION), "deviation for the size of the population. at any point during the GA, the population size will be in [population-initial-size - population-deviation-size; population-initial-size + population-deviation-size]")
      ("initial-buffer-size", po::value<uint32_t>()->default_value(DEFAULT_BUFFER_SIZE), "initial size of the buffer when randomly generated")
      ("initial-buffer-deviation-size", po::value<uint32_t>()->default_value(DEFAULT_BUFFER_DEVIATION), "random deviation for the size of the initial buffers")
      ("max-num-testcases", po::value<uint64_t>()->default_value(DEFAULT_MAX_NUM_TESTCASES), "maximum number of generated testcases")
      ("feedback-only", po::value<bool>()->default_value(false), "do not leverage goals")
      ("grammar-mutations-only", po::value<bool>()->default_value(false), "only perform mutations based on a grammar")
      ("fork-server", po::value<bool>()->default_value(false), "use the fork-server embedded in the SUT")
      ("max-num-processes", po::value<size_t>()->default_value(DEFAULT_MAX_NUM_PROCESSES), "maximum number of processes running at the same time")
      ("dump-statistics", po::value<bool>()->default_value(true), "dump statistics related to the testcase generation")
      ("slow-mating-strategies", po::value<bool>()->default_value(false), "enable mating strategies that are computing intensive")
      ("evolution-timeout-seconds", po::value<uint32_t>()->default_value(DEFAULT_EVOLUTION_TIMEOUT_SECONDS), "maximum number of seconds one evolution in the GA can take")
      ("max-evolution-fixpoint", po::value<uint32_t>()->default_value(DEFAULT_MAX_EVOLUTION_FIXPOINT), "maximum number of non-improving generations");

    po::options_description input_options("Input options");
    input_options.add_options()
      ("seeds", po::value<string>(), "path to the seeds file CSV of \"string/file,value/absolute_path\\n\"")
      ("models", po::value<string>()->default_value("models.xxx"), "path to the models files")
      ("environment", po::value<string>(), "extra environment variables to add");

    po::options_description transformation_options("Transformation options");
    transformation_options.add_options()
      ("ld-preload", po::value<string>(), "value passed to LD_PRELOAD")
      ("post-processor", po::value<string>(), "path to script for post-processing the fuzzed value");

    po::options_description target_options("Target options");
    target_options.add_options()
      ("target-timeout-seconds", po::value<uint32_t>()->default_value(DEFAULT_PROCESS_TIMEOUT_SECONDS), "maximum number of seconds the target process can run")
      ("target-symbols", po::value<string>()->default_value(""), "directory of symbols for the target binaries (generated by dump_sysm, and as .sym)")
      ("stream-target-stdout", po::value<bool>()->default_value(false), "forward the stdout/stderr of the SUT to the current console")
      ("force-crash-target", po::value<bool>()->default_value(false), "for the SUT to crash when it's called")
      ("fake-target-call", po::value<bool>()->default_value(false), "skip calling the SUT, generate random data");

    po::options_description ui_options("UI options");
    ui_options.add_options()
      ("disable-ui", po::value<bool>()->default_value(false), "no UI")
      ("ui-port", po::value<uint32_t>()->default_value(DEFAULT_UI_PORT), "port of the web UI")
      ("ui-docroot", po::value<string>()->default_value("www"), "path to the docroot")
      ("debug-loop", po::value<bool>()->default_value(false), "allow debug loop (UI -> score expectation)")
      ("pattern-watchers", po::value<bool>()->default_value(false), "allow pattern watchers (UI -> testcase pattern hit reports)");
    // clang-format on

    all_opts.add(general_options)
        .add(fuzzing_options)
        .add(input_options)
        .add(transformation_options)
        .add(target_options)
        .add(ui_options);

    po::variables_map vm;
    po::store(po::parse_command_line(new_argc, argv, all_opts), vm);
    po::notify(vm);

    if (argc < 2 || vm.count("help")) {
      cout << all_opts << endl;
      return 0;
    }

    try {
      if (vm.count("debug") && !vm["debug"].as<bool>()) {
        instr::setupLogger("fuzzing.log");
      }

      FuzzerHandler fuzzer_handler(vm);
      if (!command_line.empty()) {
        fuzzer_handler.set_command_line(command_line);
      }
      fuzzer_handler.start();
    } catch (exception &e) {
      cerr << "Top-level Exception: " << e.what() << endl;
    }

  } catch (exception &e) {
    cerr << "Exception: " << e.what() << endl;
    return 1;
  }
  return 0;
}
