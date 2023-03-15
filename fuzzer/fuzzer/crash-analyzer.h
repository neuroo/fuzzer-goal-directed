#ifndef CRASH_ANALYZER_H
#define CRASH_ANALYZER_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "bf.h"

namespace fuzz {

#define MAX_NUMBER_CRASH_PER_KIND 250

class CrashAnalyzer;

struct BreakpadDumpDecoder {
  bool skip_symbolize = false;
  fs::path symbols_dir;

  std::map<std::string, size_t> current_number_outputs;

  BreakpadDumpDecoder() = delete;
  BreakpadDumpDecoder(const BreakpadDumpDecoder &) = delete;
  BreakpadDumpDecoder &operator=(const BreakpadDumpDecoder &) = delete;

  BreakpadDumpDecoder(const bool skip_symbolize, const fs::path &symbols_dir)
      : skip_symbolize(skip_symbolize), symbols_dir(symbols_dir) {}

  bool handle_dump_file(const fs::path &dump_file, const fs::path &crashes_dir,
                        CrashAnalyzer *crash_analyzer);

  std::string create_crash_id(const std::string &reason,
                              const std::vector<uint64_t> &offset_trace);
};

struct Reproducer {
  //
};

class CrashAnalyzer {
  std::unique_ptr<BreakpadDumpDecoder> decoder;

  std::mutex crashers_mutex;
  std::unique_ptr<bf::basic_bloom_filter>
      crashers; // set of testcase ids that crash

  bool skip_symbolize = false;
  fs::path symbols_dir;

  fs::path idir;
  fs::path dumps_dir;
  fs::path crashes_dir;
  fs::path results_dir;

public:
  CrashAnalyzer() = delete;
  CrashAnalyzer(const CrashAnalyzer &) = delete;
  CrashAnalyzer &operator=(const CrashAnalyzer &) = delete;

  CrashAnalyzer(const std::string &idir_str, const std::string &symbols_str);

  bool run();

  bool is_crashing(const uint64_t testcase_id);
  void set_is_crashing(const uint64_t testcase_id);

private:
  void initialize(const std::string &idir_str, const std::string &symbols_str);
  bool handle_dump_file(const fs::path &dump_file);
};
}

#endif
