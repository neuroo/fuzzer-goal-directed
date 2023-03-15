#include "knowledge.h"
#include "common/elements.h"
#include "common/logger.h"
#include "common/store.h"
#include "utils.h"
using namespace instr;

#include <boost/graph/graphviz.hpp>
namespace bgl = boost;

#include <fstream>
#include <map>
#include <memory>
#include <string>
using namespace std;

namespace fuzz {

#define LRU_CACHE_SIZE 15000
#define MAX_BLIND_NUM_FUNC 65535

//
// ProgramKnowledge
//
ProgramKnowledge::ProgramKnowledge(const string &models_file)
    : blind(false), store(new Store(models_file)),
      block_cache(new lru_cache_t(LRU_CACHE_SIZE)) {
  LOG(INFO) << store->toString();
  initialize();
}

// This is a dummy constructor that does not construct a store.
ProgramKnowledge::ProgramKnowledge(const bool blind) : blind(blind) {
  initialize();
  create_mocking_random();
}

void ProgramKnowledge::initialize() {
  coverage = std::unique_ptr<Coverage>(new Coverage(*this));
}

void ProgramKnowledge::create_mocking_random() {
  random = std::unique_ptr<utils::Rand>(new utils::Rand(time(nullptr)));
}

//
// All these methods are dispatches for coverage. That's not useful...
//
void ProgramKnowledge::add_trace(const uint64_t testcase_id,
                                 shm::Container::trace_t &trace) {
  coverage->add_trace(testcase_id, trace);
}

measure::trace_score_t
ProgramKnowledge::evaluate_trace(const uint64_t testcase_id,
                                 shm::Container::trace_t &trace) {
  return coverage->evaluate_trace(testcase_id, trace);
}

void ProgramKnowledge::add_trace(const uint64_t testcase_id,
                                 shm::Container::mocked_trace_t &trace) {
  coverage->add_trace(testcase_id, trace);
}

const std::map<instr::element_id, uint32_t> &
ProgramKnowledge::get_local_coverage() const {
  return coverage->get_local_coverage();
}

void ProgramKnowledge::to_dot(const std::string &filename) {
  coverage->to_dot(filename);
}

std::pair<uint32_t, uint32_t> ProgramKnowledge::coverage_size() {
  return coverage->size();
}

const score_map &ProgramKnowledge::get_coverage_scores() const {
  return coverage->get_coverage_scores();
}

const score_map &ProgramKnowledge::get_goals_scores() const {
  return coverage->get_goals_scores();
}

void ProgramKnowledge::reset_scores() { coverage->reset_scores(); }

// That's a lot of work to get that... The LRU cache is just a poor man
// solution currently used.
element_id ProgramKnowledge::get_block_element(const element_id func_id,
                                               const uint32_t block_id) {
  if (blind) {
    // Use Szudzik's encoding, don't cache anything
    if (func_id > MAX_BLIND_NUM_FUNC || block_id > MAX_BLIND_NUM_FUNC) {
      LOG(ERROR) << "Szudzik's pairing cannot be used without conflicts, more "
                    "than 65535 functions or blocks in function.";
    }
    return func_id >= block_id ? func_id * func_id + func_id + block_id
                               : func_id + block_id * block_id;
  }

  const lru_cache_key_t cache_key = lru_cache_key_t(func_id, block_id);
  if (block_cache->exists(cache_key)) {
    return block_cache->copy_get(cache_key);
  }

  auto elmt = elements()[func_id];
  if (!elmt) {
    LOG(ERROR) << "Unknown function " << func_id;
    return ERROR_ID;
  }

  if (elmt->getKind() != Element::E_FUNCTION) {
    LOG(ERROR) << "Got a weird kind for func_id: " << elmt->getKind();
    block_cache->put(cache_key, ERROR_ID);
    return ERROR_ID;
  }

  auto func_elmt = std::static_pointer_cast<FunctionElement>(elmt);

  for (auto &block_elmt_id : func_elmt->blocks) {
    auto be = elements()[block_elmt_id];
    auto block_elmt = std::static_pointer_cast<BlockElement>(be);
    if (block_elmt->internal_block_id == block_id) {
      block_cache->put(cache_key, block_elmt_id);
      return block_elmt_id;
    }
  }

  LOG(ERROR) << "Couldn't find the block " << block_id
             << " for the given func_id: " << func_id;
  block_cache->put(cache_key, ERROR_ID);
  return ERROR_ID;
}

//
// Coverage methods
//
Coverage::Coverage(ProgramKnowledge &knowledge)
    : knowledge(knowledge), vertex_cache(new lru_cache_t(LRU_CACHE_SIZE)) {}

void Coverage::add_trace(const uint64_t testcase_id,
                         shm::Container::trace_t &trace) {
  size_t num_elements = 0;
  for (auto &trace_element : trace) {
    add_trace_element(testcase_id, trace_element);
    num_elements++;
  }
  LOG(INFO) << "Coverage: testcase_id=" << testcase_id
            << " trace_size=" << num_elements;
}

void Coverage::add_trace(const uint64_t testcase_id,
                         shm::Container::mocked_trace_t &trace) {
  update_coverage_score(testcase_id, 0, 0, /*initialize*/ true);
  update_goal_score(testcase_id, 0, 0, /*initialize*/ true);

  for (auto &trace_element : trace) {
    add_trace_element(testcase_id, trace_element);
  }
}

measure::trace_score_t
Coverage::evaluate_trace(const uint64_t testcase_id,
                         shm::Container::trace_t &trace) {
  measure::trace_score_t result;

  std::list<instr::element_id> trace_list;
  measure::measure_t m_result;

  update_coverage_score(testcase_id, 0, 0, /*initialize*/ true);
  update_goal_score(testcase_id, 0, 0, /*initialize*/ true);

  for (auto &trace_element : trace) {
    add_trace_element(testcase_id, trace_element, /*mock*/ true, &trace_list);
  }

  // We don't have the size here...
  m_result.goal = goal_scores[testcase_id];
  m_result.edge = coverage_scores[testcase_id];

  result.first = trace_list;
  result.second = m_result;

  return result;
}

void Coverage::add_trace_element(const uint64_t testcase_id,
                                 shm::TraceElement &trace_element, bool mock,
                                 std::list<instr::element_id> *trace_list_ptr) {
  if (trace_element.cur_block_id == 0) {
    // Record the visited functions in func_enter/func_exit boundaries
    if (trace_element.func_id) {
      // If it's a new function, increase score and keep the function
      update_local_coverage(trace_element.func_id);

      if (reached_functions.find(trace_element.func_id) ==
          reached_functions.end()) {
        update_coverage_score(testcase_id, /*absolute*/ 2, /*diff*/ 1);
        reached_functions.insert(trace_element.func_id);
      } else {
        update_coverage_score(testcase_id, /*absolute*/ 1, /*diff*/ 0);
      }
    }
    return;
  }
  const element_id pred_block_elmt_id = knowledge.get_block_element(
      trace_element.func_id, trace_element.pred_block_id);
  const element_id cur_block_elmt_id = knowledge.get_block_element(
      trace_element.func_id, trace_element.cur_block_id);

  if (trace_list_ptr != nullptr) {
    trace_list_ptr->push_back(cur_block_elmt_id);
  }

  add_edge(pred_block_elmt_id, cur_block_elmt_id, testcase_id, mock);
}

// XXX booo! really need to do something for that, caching again???
Coverage::vertex_t Coverage::find_vertex(const element_id element,
                                         bool &found) {
  vertex_iterator b, e;
  found = false;
  boost::tie(b, e) = bgl::vertices(graph);
  for (; b != e; ++b) {
    if (graph[*b].element == element) {
      found = true;
      return *b;
    }
  }
  return *e;
}

Coverage::vertex_t Coverage::add_vertex(const element_id element) {
  bool found = false;

  const lru_cache_key_t cache_key = element;
  if (vertex_cache->exists(cache_key)) {
    return vertex_cache->copy_get(cache_key);
  }

  vertex_t t = find_vertex(element, found);
  if (!found) {
    vertex_t v = bgl::add_vertex(graph);
    graph[v].element = element;
    vertex_cache->put(cache_key, v);
    return v;
  } else {
    vertex_cache->put(cache_key, t);
    return t;
  }
}

void Coverage::add_edge(const element_id source, const element_id dest,
                        const uint64_t testcase_id, bool mock) {
  vertex_t v_source = add_vertex(source);
  vertex_t v_dest = add_vertex(dest);

  update_local_coverage(source);
  update_local_coverage(dest);

  auto edge = bgl::edge(v_source, v_dest, graph);
  if (!edge.second) {
    // Add the edge into the graph & update the score by the same token
    LOG(INFO) << "Reached new block from testcase #" << testcase_id
              << " (element_ids " << source << "->" << dest << ")";
    update_coverage_score(testcase_id, /*absolute*/ 2, /*diff*/ 1);
    if (!mock) {
      bgl::add_edge(v_source, v_dest, graph);
    }
  } else {
    update_coverage_score(testcase_id, /*absolute*/ 1, /*diff*/ 0);
  }
  lookup_goals(source, dest, testcase_id);
}

void Coverage::lookup_goals(const instr::element_id source,
                            const instr::element_id dest,
                            const uint64_t testcase_id) {
  auto goal_scores = compute_goal_score(dest);
  if (goal_scores.norm() > 0) {
    if (goal_scores.diff > 0) {
      LOG(INFO) << " [+] New goals from testcase #" << testcase_id;
    }
    update_goal_score(testcase_id, /*absolute*/ goal_scores.absolute,
                      /*diff*/ goal_scores.diff);
  }
}

score_t Coverage::compute_goal_score(const instr::element_id block_id) {
  if (knowledge.blind) {
    return compute_mocked_score(block_id);
  }

  score_t score;
  auto elmt = knowledge.elements()[block_id];
  if (!elmt) {
    LOG(INFO) << "No goal score for block: " << block_id;
    return score;
  }

  if (elmt->getKind() != Element::E_BLOCK) {
    LOG(ERROR) << "Element is not a block: " << elmt->toString();
    return score;
  }

  auto block_elmt = std::static_pointer_cast<BlockElement>(elmt);

  for (auto &summary_elmt_id : block_elmt->summaries) {
    const bool is_new_goal =
        covered_goals.find(summary_elmt_id) == covered_goals.end();
    auto s_elmt = knowledge.elements()[summary_elmt_id];
    covered_goals.insert(summary_elmt_id);
    if (!s_elmt) {
      LOG(ERROR) << "Cannot find summary element specified in the block...";
      continue;
    }
    if (s_elmt->getKind() != Element::E_SUMMARY)
      continue;
    auto summary_elmt = std::static_pointer_cast<SummaryElement>(s_elmt);
    uint32_t element_score = goal_scoring(summary_elmt);
    score.absolute += element_score;
    if (is_new_goal) {
      LOG(INFO) << "Reached new goal: " << summary_elmt->toString();
      score.diff += element_score;
    }
  }
  return score;
}

score_t Coverage::compute_mocked_score(const instr::element_id block_id) {
  auto mock_iter = mocked_scores.find(block_id);
  if (mock_iter != mocked_scores.end()) {
    return mock_iter->second;
  }

  score_t score;

  // Otherwise, we randomly generate a score for this block_id
  if (knowledge.random->next_number(30) > 0) {
    mocked_scores.insert({block_id, score});
    return score;
  } else {
    uint32_t result = 0;
    uint32_t num_hits = knowledge.random->next_number(5);
    for (uint32_t i = 0; i < num_hits; i++) {
      result += knowledge.random->next_number(3);
    }
    mocked_scores.insert(
        {block_id, score_t(knowledge.random->next_number(10), result)});
    return score_t(knowledge.random->next_number(10), result);
  }
}

void Coverage::to_dot(const std::string &filename) {
  ofstream out(filename);
  bgl::write_graphviz(out, graph, bgl::make_label_writer(bgl::get(
                                      &vertex_bundle_t::element, graph)));
  out.close();
}

pair<uint32_t, uint32_t> Coverage::size() const {
  return make_pair(bgl::num_vertices(graph), bgl::num_edges(graph));
}

void Coverage::update_local_coverage(const instr::element_id element) {
  auto iter = local_coverage.find(element);
  if (iter == local_coverage.end()) {
    local_coverage.insert({element, 1});
  } else {
    iter->second += 1;
  }
}

void Coverage::update_coverage_score(const uint64_t testcase_id,
                                     const uint32_t abs_score,
                                     const uint32_t diff_score,
                                     bool initialize) {
  auto iter = coverage_scores.find(testcase_id);
  if (iter == coverage_scores.end()) {
    coverage_scores.insert({testcase_id, score_t(abs_score, diff_score)});
  } else {
    if (initialize) {
      iter->second = score_t(abs_score, diff_score);
    } else {
      iter->second.absolute += abs_score;
      iter->second.diff += diff_score;
    }
  }
}

void Coverage::update_goal_score(const uint64_t testcase_id,
                                 const uint32_t abs_score,
                                 const uint32_t diff_score, bool initialize) {
  auto iter = goal_scores.find(testcase_id);
  if (iter == goal_scores.end()) {
    goal_scores.insert({testcase_id, score_t(abs_score, diff_score)});
  } else {
    if (initialize) {
      iter->second = score_t(abs_score, diff_score);
    } else {
      iter->second.absolute += abs_score;
      iter->second.diff += diff_score;
    }
  }
}

void Coverage::reset_scores() {
  coverage_scores.clear();
  goal_scores.clear();
  local_coverage.clear();
}

//
// GoalScoringMechanism
//
uint32_t GoalScoringMechanism::
operator()(const std::shared_ptr<SummaryElement> &e) const {
  switch (e->op) {
  case OP_PASS_THROUGH: // a function call is very interesting!
    return 10;
  case OP_BUFFER_WRITE:
    return 7;
  case OP_BUFFER_READ:
  case OP_BUFFER_READ_WRITE:
    return 3;
  case OP_BUFFER_UNKNOWN:
    return 2;
  case OP_INTEGER_MAY_OVERFLOW:
  case OP_INTEGER_UNKNOWN:
    return 2;
  case OP_CAST_UNSAFE:
    return 2;
  case OP_CAST_UNKNOWN:
    return 1;
  default:
    return 0;
  }
}

#undef LRU_CACHE_SIZE
}
