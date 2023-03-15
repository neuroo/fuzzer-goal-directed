#include "crash-analyzer.h"
#include "commander.h"
#include "common/logger.h"
#include "utils.h"

#include <boost/filesystem.hpp>
#include <boost/predef.h>
#include <boost/process.hpp>
#include <boost/tokenizer.hpp>
namespace bp = boost::process;
namespace fs = boost::filesystem;

#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "processor/simple_symbol_supplier.h"

#include "json/json.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using namespace std;

namespace fuzz {

static const std::string ENV_TESTCASE_ID_EQ = "COVERAGE_FUZZING_TESTCASE_ID=";

CrashAnalyzer::CrashAnalyzer(const string &idir_str,
                             const string &symbols_str) {
  initialize(idir_str, symbols_str);
}

void mkdir_p(const fs::path &d) {
  try {
    fs::create_directories(d);
  } catch (exception &ex) {
    //
  }
}

void CrashAnalyzer::initialize(const string &idir_str,
                               const string &symbols_str) {
  idir = fs::path(idir_str);
  dumps_dir = idir / "dumps";
  crashes_dir = idir / "crashes";
  results_dir = idir / "results";

  mkdir_p(dumps_dir);
  mkdir_p(crashes_dir);
  mkdir_p(results_dir);

  // Now register the symbols and make sure it's an actual file.
  symbols_dir = fs::path(symbols_str);
  if (!fs::is_directory(symbols_dir)) {
    LOG(ERROR) << "Cannot find the symbols directory: " << symbols_dir;
    skip_symbolize = true;
  }

  // Create the decoder
  decoder = std::unique_ptr<BreakpadDumpDecoder>(
      new BreakpadDumpDecoder(skip_symbolize, symbols_dir));

  crashers = std::unique_ptr<bf::basic_bloom_filter>(
      new bf::basic_bloom_filter(bf::make_hasher(3), 65536));
}

bool CrashAnalyzer::is_crashing(const uint64_t testcase_id) {
  std::lock_guard<std::mutex> lock(crashers_mutex);
  return crashers->lookup(testcase_id) == 1;
}

void CrashAnalyzer::set_is_crashing(const uint64_t testcase_id) {
  std::lock_guard<std::mutex> lock(crashers_mutex);
  return crashers->add(testcase_id);
}

bool CrashAnalyzer::run() {
  LOG(INFO) << "Inspecting for dumps...";

  if (!fs::is_directory(dumps_dir)) {
    LOG(ERROR) << "Dumps directory " << dumps_dir << " doesn't exist.";
    return false;
  }

  try {
    for (auto &x : fs::directory_iterator(dumps_dir)) {
      const fs::path &current_path = x.path();
      if (fs::is_regular_file(current_path) &&
          current_path.extension() == ".dmp") {
        if (decoder->handle_dump_file(current_path, crashes_dir, this)) {
          fs::remove(current_path);
        }
      }
    }
  } catch (exception &e) {
    LOG(ERROR) << "Exception: " << e.what();
    return false;
  }
  return true;
}

// Sadly it doesn't seem the minidump interface allows for better
// way to get access to the environment variables
static uint64_t get_testcase_id(const char *dump_filename) {
  if (dump_filename == nullptr)
    return 0;
  uint64_t testcase_id = 0;
  ifstream stream(dump_filename, std::ios::in | std::ios::binary);

  size_t pos = string::npos;
  string line;
  bool errored = false;
  while (!stream.eof()) {
    line.clear();
    stream >> line;
    if ((pos = line.find(ENV_TESTCASE_ID_EQ)) != string::npos)
      break;
  }
  stream.close();

  if (pos != string::npos) {
    pos += ENV_TESTCASE_ID_EQ.length();
    const char *buffer = line.c_str();
    const size_t length = line.length();
    while (pos < length) {
      const char c = buffer[pos];
      if (std::isdigit(c)) {
        testcase_id = testcase_id * 10 + (c - '0');
      } else {
        break;
      }
      ++pos;
    }
  }
  return testcase_id;
}

static string to_hex(uint64_t value) {
  char buffer[17];
  sprintf(buffer, "0x%llx", value);
  return buffer;
}

static string to_int(uint64_t value) {
  char buffer[17];
  sprintf(buffer, "%lld", value);
  return buffer;
}

//
// Stolen from Mozilla's socorro:
//   https://github.com/mozilla/socorro
//
void AddRegister(Json::Value &registers, const char *reg, uint32_t value) {
  char buf[11];
  snprintf(buf, sizeof(buf), "0x%08x", value);
  registers[reg] = buf;
}

void AddRegister(Json::Value &registers, const char *reg, uint64_t value) {
  char buf[19];
  snprintf(buf, sizeof(buf), "0x%016llx", value);
  registers[reg] = buf;
}

// Save all the registers from |frame| of CPU type |cpu|
// into keys in |registers|.
void RegistersToJSON(const google_breakpad::StackFrame *frame,
                     const string &cpu, Json::Value &registers) {
  using namespace google_breakpad;

  if (cpu == "x86") {
    const StackFrameX86 *frame_x86 =
        reinterpret_cast<const StackFrameX86 *>(frame);

    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EIP)
      AddRegister(registers, "eip", frame_x86->context.eip);
    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESP)
      AddRegister(registers, "esp", frame_x86->context.esp);
    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBP)
      AddRegister(registers, "ebp", frame_x86->context.ebp);
    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBX)
      AddRegister(registers, "ebx", frame_x86->context.ebx);
    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESI)
      AddRegister(registers, "esi", frame_x86->context.esi);
    if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EDI)
      AddRegister(registers, "edi", frame_x86->context.edi);
    if (frame_x86->context_validity == StackFrameX86::CONTEXT_VALID_ALL) {
      AddRegister(registers, "eax", frame_x86->context.eax);
      AddRegister(registers, "ecx", frame_x86->context.ecx);
      AddRegister(registers, "edx", frame_x86->context.edx);
      AddRegister(registers, "efl", frame_x86->context.eflags);
    }
  } else if (cpu == "amd64") {
    const StackFrameAMD64 *frame_amd64 =
        reinterpret_cast<const StackFrameAMD64 *>(frame);

    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RAX)
      AddRegister(registers, "rax", frame_amd64->context.rax);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RDX)
      AddRegister(registers, "rdx", frame_amd64->context.rdx);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RCX)
      AddRegister(registers, "rcx", frame_amd64->context.rcx);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RBX)
      AddRegister(registers, "rbx", frame_amd64->context.rbx);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RSI)
      AddRegister(registers, "rsi", frame_amd64->context.rsi);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RDI)
      AddRegister(registers, "rdi", frame_amd64->context.rdi);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RBP)
      AddRegister(registers, "rbp", frame_amd64->context.rbp);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RSP)
      AddRegister(registers, "rsp", frame_amd64->context.rsp);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R8)
      AddRegister(registers, "r8", frame_amd64->context.r8);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R9)
      AddRegister(registers, "r9", frame_amd64->context.r9);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R10)
      AddRegister(registers, "r10", frame_amd64->context.r10);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R11)
      AddRegister(registers, "r11", frame_amd64->context.r11);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R12)
      AddRegister(registers, "r12", frame_amd64->context.r12);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R13)
      AddRegister(registers, "r13", frame_amd64->context.r13);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R14)
      AddRegister(registers, "r14", frame_amd64->context.r14);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_R15)
      AddRegister(registers, "r15", frame_amd64->context.r15);
    if (frame_amd64->context_validity & StackFrameAMD64::CONTEXT_VALID_RIP)
      AddRegister(registers, "rip", frame_amd64->context.rip);
  }
}

static Json::Value get_crashed_trace(google_breakpad::CallStack *call_stack,
                                     const string &cpu,
                                     vector<uint64_t> &offset_trace) {
  using namespace google_breakpad;
  if (!call_stack) {
    return Json::Value(Json::nullValue);
  }

  Json::Value trace_root(Json::arrayValue);
  int frame_count = call_stack->frames()->size();
  for (int frame_num = 0; frame_num < frame_count; frame_num++) {
    const StackFrame *frame = call_stack->frames()->at(frame_num);
    Json::Value frame_data;
    frame_data["frame"] = frame_num;

    if (frame) {
      frame_data["trust"] = frame->trust_description();
      frame_data["offset"] = to_hex(frame->instruction);
      frame_data["function_name"] = frame->function_name;
      frame_data["function_base"] = to_hex(frame->instruction);
      frame_data["source_file_name"] = frame->source_file_name;
      frame_data["source_line"] = frame->source_line;
      frame_data["source_line_base"] = frame->source_line_base;

      if (frame->module) {
        frame_data["file"] = frame->module->code_file();
        frame_data["file_identifier"] = frame->module->code_identifier();
        frame_data["file_base_address"] = to_hex(frame->module->base_address());
      }
      Json::Value registers;
      RegistersToJSON(frame, cpu, registers);
      frame_data["registers"] = registers;

      offset_trace.push_back(frame->instruction -
                             frame->module->base_address());
    } else {
      offset_trace.push_back(frame_num);
    }

    trace_root.append(frame_data);
  }
  return trace_root;
}

// Return a unique string that encodes the reason of the crash
string
BreakpadDumpDecoder::create_crash_id(const string &reason,
                                     const vector<uint64_t> &offset_trace) {
  ostringstream oss;
  oss << reason << ":";
  for (uint64_t offset : offset_trace) {
    oss << to_hex(offset) << ",";
  }
  const string final = oss.str();
  const size_t length = final.length();
  const uint8_t *data = reinterpret_cast<const uint8_t *>(final.c_str());
  utils::container::uint128_t hash =
      utils::hash(const_cast<uint8_t *>(data), length);

  return hash.str(/*base*/ 16);
}

bool BreakpadDumpDecoder::handle_dump_file(const fs::path &dump_file,
                                           const fs::path &crashes_dir,
                                           CrashAnalyzer *crash_analyzer) {
  LOG(INFO) << "Handle dump file: " << dump_file;
  using namespace google_breakpad;

  const char *dump_filename = dump_file.string().c_str();
  const uint64_t testcase_id = get_testcase_id(dump_filename);

  Minidump minidump(dump_filename);
  minidump.Read();

  BasicSourceLineResolver *resolver = nullptr;
  SimpleSymbolSupplier *symbolizer = nullptr;

  if (!skip_symbolize) {
    resolver = new BasicSourceLineResolver();
    symbolizer = new SimpleSymbolSupplier(symbols_dir.string());
  }

  MinidumpProcessor processor(symbolizer, resolver,
                              /*enable_exploitability*/ false);

  ProcessState process_state;
  ProcessResult result = processor.Process(&minidump, &process_state);

  Json::Value root;
  root["testcase_id"] = testcase_id;
  root["crash_file"] = dump_file.filename().string();

  Json::Value crash_info;
  crash_info["time"] = process_state.time_date_stamp();
  crash_info["crashed"] = process_state.crashed();
  crash_info["reason"] = process_state.crash_reason();
  crash_info["address"] = to_hex(process_state.crash_address());
  crash_info["assertion"] = process_state.assertion();
  crash_info["thread"] = process_state.requesting_thread();
  crash_info["exploitability"] = process_state.exploitability();

  root["crash_info"] = crash_info;

  string crash_reason = process_state.crash_reason();
  vector<uint64_t> offset_trace;

  if (const vector<CallStack *> *call_stack = process_state.threads()) {
    try {
      root["trace"] =
          get_crashed_trace(call_stack->at(process_state.requesting_thread()),
                            process_state.system_info()->cpu, offset_trace);
    } catch (exception &ex) {
      LOG(ERROR) << "Cannot get trace. Exception- " << ex.what();
    }
  } else {
    root["trace"] = Json::Value(Json::nullValue);
  }

  try {
    const string crash_id = create_crash_id(crash_reason, offset_trace);
    crash_analyzer->set_is_crashing(testcase_id);

    auto c_iter = current_number_outputs.find(crash_id);
    bool create_info = true;
    if (c_iter == current_number_outputs.end()) {
      current_number_outputs.insert({crash_id, 1});
    } else {
      c_iter->second = c_iter->second + 1;
      if (c_iter->second > MAX_NUMBER_CRASH_PER_KIND) {
        create_info = false;
      }
    }

    if (create_info) {
      fs::path local_crash_dir = crashes_dir / crash_id;
      local_crash_dir = local_crash_dir / std::to_string(testcase_id);
      mkdir_p(local_crash_dir);
      fs::path copy_dump_file = local_crash_dir / dump_file.filename();

      fs::copy_file(dump_file, copy_dump_file,
                    fs::copy_option::overwrite_if_exists);

      fs::path crash_file = local_crash_dir / "info.json";

      Json::Writer *writer = new Json::StyledWriter();

      ofstream ofs(crash_file.c_str(), std::ios::out);
      ofs << writer->write(root).c_str();
      ofs.close();

      delete writer;
    }
  } catch (exception &ex) {
    LOG(ERROR) << "Exception- " << ex.what();
  }

  if (resolver)
    delete resolver;
  if (symbolizer)
    delete symbolizer;

  return true;
}
}
