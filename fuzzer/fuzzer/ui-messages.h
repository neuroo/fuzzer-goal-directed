#ifndef UI_MESSAGES_H
#define UI_MESSAGES_H

#include "common/elements.h"
#include "evolution.h"
#include "knowledge.h"
#include "measure.h"
#include "utils.h"

#include "json/json.h"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace fuzz {

namespace ui {

typedef measure::measure_t measure_t;
typedef measure::trace_score_t trace_score_t;

//
// ServerMessage is the root of the JSON messages we send back to the UI
//
struct ServerMessage {
  enum MessageKind {
    E_MESSAGE_PROGRESS = 0,
    E_MESSAGE_SCORES,
    E_MESSAGE_COVERAGE,
    E_MESSAGE_TESTCASES_PROB,
    E_MESSAGE_TESTCASES,
    E_MESSAGE_CRASHERS,
    E_MESSAGE_GLOBAL,
    E_MESSAGE_TARGET,
    E_MESSAGE_ELEMENT_FUNCTION,
    E_MESSAGE_ELEMENT_BLOCK,
    E_MESSAGE_ELEMENT_GOAL,
    E_MESSAGE_DEBUG_LOOP,
    E_MESSAGE_STATISTICS,
    E_MESSAGE_UNKNOWN = 0xff
  };

  MessageKind message_kind = E_MESSAGE_UNKNOWN;

  ServerMessage() = delete;
  ServerMessage(const MessageKind message_kind) : message_kind(message_kind) {}
  ServerMessage(const ServerMessage &m) : message_kind(m.message_kind) {}
  ServerMessage &operator=(const ServerMessage &m) {
    if (this != &m) {
      message_kind = m.message_kind;
    }
    return *this;
  }

  MessageKind getKind() const { return message_kind; }

  virtual Json::Value to_json() const { return Json::Value(Json::nullValue); };
};

struct ProgressMessage : public ServerMessage {
  double elapsed_seconds = 0.0;
  uint64_t generated_testcases;

  ProgressMessage() : ServerMessage(ServerMessage::E_MESSAGE_PROGRESS) {}

  ProgressMessage(const double elapsed_seconds,
                  const uint64_t generated_testcases)
      : ServerMessage(ServerMessage::E_MESSAGE_PROGRESS),
        elapsed_seconds(elapsed_seconds),
        generated_testcases(generated_testcases) {}

  ProgressMessage(const ProgressMessage &m)
      : ServerMessage(m), elapsed_seconds(m.elapsed_seconds),
        generated_testcases(m.generated_testcases) {}

  ProgressMessage &operator=(const ProgressMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    elapsed_seconds = m.elapsed_seconds;
    generated_testcases = m.generated_testcases;
    return *this;
  }

  Json::Value to_json() const;
};

struct ScoreMessage : public ServerMessage {
  uint64_t index;
  measure_t min_score;
  measure_t max_score;

  ScoreMessage() : ServerMessage(ServerMessage::E_MESSAGE_SCORES) {}

  ScoreMessage(const uint64_t index, const measure_t &min_score,
               const measure_t &max_score)
      : ServerMessage(ServerMessage::E_MESSAGE_SCORES), index(index),
        min_score(min_score), max_score(max_score) {}

  ScoreMessage(const ScoreMessage &m)
      : ServerMessage(m), index(m.index), min_score(m.min_score),
        max_score(m.max_score) {}

  ScoreMessage &operator=(const ScoreMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    index = m.index;
    min_score = m.min_score;
    max_score = m.max_score;
    return *this;
  }

  Json::Value to_json() const;
};

struct GlobalMessage : public ServerMessage {
  std::string command;
  std::string idir;
  std::map<std::string, std::string> options;

  GlobalMessage() : ServerMessage(ServerMessage::E_MESSAGE_GLOBAL) {}

  GlobalMessage(const std::string &command, const std::string &idir,
                const std::map<std::string, std::string> &options)
      : ServerMessage(ServerMessage::E_MESSAGE_GLOBAL), command(command),
        idir(idir), options(options) {}

  GlobalMessage(const GlobalMessage &m)
      : ServerMessage(m), command(m.command), idir(m.idir), options(m.options) {
  }

  GlobalMessage &operator=(const GlobalMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    command = m.command;
    idir = m.idir;
    options = m.options;
    return *this;
  }

  Json::Value to_json() const;
};

struct TargetMessage : public ServerMessage {
  typedef std::map<instr::element_id, std::string> elements_kind;

  uint32_t num_functions;
  uint32_t num_blocks;
  uint32_t num_goals;
  elements_kind elements;

  TargetMessage() : ServerMessage(ServerMessage::E_MESSAGE_TARGET) {}

  TargetMessage(const uint32_t num_functions, const uint32_t num_blocks,
                const uint32_t num_goals, const elements_kind &elements)
      : ServerMessage(ServerMessage::E_MESSAGE_TARGET),
        num_functions(num_functions), num_blocks(num_blocks),
        num_goals(num_goals), elements(elements) {}

  TargetMessage(const TargetMessage &m)
      : ServerMessage(m), num_functions(m.num_functions),
        num_blocks(m.num_blocks), num_goals(m.num_goals), elements(m.elements) {
  }

  TargetMessage &operator=(const TargetMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    num_functions = m.num_functions;
    num_blocks = m.num_blocks;
    num_goals = m.num_goals;
    elements = m.elements;
    return *this;
  }

  Json::Value to_json() const;
};

struct ElementFunctionMessage : public ServerMessage {
  uint32_t id;
  std::string name;
  std::vector<instr::element_id> blocks;

  ElementFunctionMessage()
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_FUNCTION) {}

  ElementFunctionMessage(const uint32_t id, const std::string &name,
                         const std::vector<instr::element_id> &blocks)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_FUNCTION), id(id),
        name(name), blocks(blocks) {}

  ElementFunctionMessage(const ElementFunctionMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_FUNCTION), id(m.id),
        name(m.name), blocks(m.blocks) {}

  ElementFunctionMessage &operator=(const ElementFunctionMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    id = m.id;
    name = m.name;
    blocks = m.blocks;
    return *this;
  }

  Json::Value to_json() const;
};

struct ElementBlockMessage : public ServerMessage {
  uint32_t id;
  uint32_t block_id;
  std::vector<instr::element_id> goals;

  ElementBlockMessage()
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_BLOCK) {}

  ElementBlockMessage(const uint32_t id, const uint32_t block_id,
                      const std::vector<instr::element_id> &goals)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_BLOCK), id(id),
        block_id(block_id), goals(goals) {}

  ElementBlockMessage(const ElementBlockMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_BLOCK), id(m.id),
        block_id(m.block_id), goals(m.goals) {}

  ElementBlockMessage &operator=(const ElementBlockMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    id = m.id;
    block_id = m.block_id;
    goals = m.goals;
    return *this;
  }

  Json::Value to_json() const;
};

struct ElementGoalMessage : public ServerMessage {
  uint32_t id;
  std::string type;
  std::string op;

  ElementGoalMessage() : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_GOAL) {}

  ElementGoalMessage(const uint32_t id, const std::string &type,
                     const std::string &op)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_GOAL), id(id),
        type(type), op(op) {}

  ElementGoalMessage(const ElementGoalMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_ELEMENT_GOAL), id(m.id),
        type(m.type), op(m.op) {}

  ElementGoalMessage &operator=(const ElementGoalMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    id = m.id;
    type = m.type;
    op = m.op;
    return *this;
  }

  Json::Value to_json() const;
};

struct CoverageMessage : public ServerMessage {
  uint32_t max_coverage = 0;
  uint32_t min_coverage = 0;
  coverage_t coverage;

  CoverageMessage() : ServerMessage(ServerMessage::E_MESSAGE_COVERAGE) {}

  CoverageMessage(const uint32_t max_coverage, const uint32_t min_coverage,
                  const coverage_t &coverage)
      : ServerMessage(ServerMessage::E_MESSAGE_COVERAGE),
        max_coverage(max_coverage), min_coverage(min_coverage),
        coverage(coverage) {}

  CoverageMessage(const CoverageMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_COVERAGE),
        max_coverage(m.max_coverage), min_coverage(m.min_coverage),
        coverage(m.coverage) {}

  CoverageMessage &operator=(const CoverageMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    max_coverage = m.max_coverage;
    min_coverage = m.min_coverage;
    coverage = m.coverage;
    return *this;
  }

  Json::Value to_json() const;
};

struct CrashersMessage : public ServerMessage {
  std::map<std::string, std::set<uint64_t>> crash_ids;

  CrashersMessage() : ServerMessage(ServerMessage::E_MESSAGE_CRASHERS) {}

  CrashersMessage(const std::map<std::string, std::set<uint64_t>> &crash_ids)
      : ServerMessage(ServerMessage::E_MESSAGE_CRASHERS), crash_ids(crash_ids) {
  }

  CrashersMessage(const CrashersMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_CRASHERS),
        crash_ids(m.crash_ids) {}

  CrashersMessage &operator=(const CrashersMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    crash_ids = m.crash_ids;
    return *this;
  }

  Json::Value to_json() const;
};

struct DebugLoopMessage : public ServerMessage {
  std::string input;
  trace_score_t trace_score;

  DebugLoopMessage() : ServerMessage(ServerMessage::E_MESSAGE_DEBUG_LOOP) {}

  DebugLoopMessage(const std::string &input, const trace_score_t &trace_score)
      : ServerMessage(ServerMessage::E_MESSAGE_DEBUG_LOOP), input(input),
        trace_score(trace_score) {}

  DebugLoopMessage(const DebugLoopMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_DEBUG_LOOP), input(m.input),
        trace_score(m.trace_score) {}

  DebugLoopMessage &operator=(const DebugLoopMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    input = m.input;
    trace_score = m.trace_score;
    return *this;
  }

  Json::Value to_json() const;
};

struct StatisticsMessage : public ServerMessage {
  struct SimpleTestCase {
    uint32_t position;
    uint32_t length;
    std::string repr;
    measure_t score;

    SimpleTestCase() = default;
    SimpleTestCase(const uint32_t position, const uint32_t length,
                   const std::string &repr, const measure_t &score)
        : position(position), length(length), repr(repr), score(score) {}
    SimpleTestCase(const SimpleTestCase &s)
        : position(s.position), length(s.length), repr(s.repr), score(s.score) {
    }
    SimpleTestCase &operator=(const SimpleTestCase &s) {
      if (&s != this) {
        position = s.position;
        length = s.length;
        repr = s.repr;
        score = s.score;
      }
      return *this;
    }

    Json::Value to_json() const;
  };

  std::map<std::string, float> stats;
  std::vector<SimpleTestCase> best;

  StatisticsMessage() : ServerMessage(ServerMessage::E_MESSAGE_STATISTICS) {}

  StatisticsMessage(
      const std::map<std::string, float> &stats,
      const std::vector<std::pair<measure_t, ga::ShareableIndividual>>
          &best_inds)
      : ServerMessage(ServerMessage::E_MESSAGE_STATISTICS), stats(stats) {
    uint32_t pos = 1;
    for (auto &x : best_inds) {
      const measure_t &score = x.first;
      const ga::ShareableIndividual &ind = x.second;
      const uint32_t length = ind.length;
      best.push_back(SimpleTestCase(
          pos, length, utils::hex_dump(ind.buffer, length), score));
      pos++;
    }
  }

  StatisticsMessage(const StatisticsMessage &m)
      : ServerMessage(ServerMessage::E_MESSAGE_STATISTICS), stats(m.stats),
        best(m.best) {}

  StatisticsMessage &operator=(const StatisticsMessage &m) {
    if (&m == this)
      return *this;
    ServerMessage::operator=(m);
    stats = m.stats;
    best = m.best;
    return *this;
  }

  Json::Value to_json() const;
};

// end namespace=ui
}
// end namespace=fuzz
}

#endif
