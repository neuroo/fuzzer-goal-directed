#ifndef KNOWLEDGE_H
#define KNOWLEDGE_H
#include <boost/config.hpp>

#include "common/elements.h"
#include "common/store.h"
#include "lrucache.h"
#include "measure.h"
#include "shared-data/shared-data.h"
#include "utils.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/property_map.hpp>
namespace bgl = boost;

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
namespace fuzz {
typedef measure::index_map index_map;
typedef measure::score_map score_map;
typedef measure::score_t score_t;
typedef measure::trace_score_t trace_score_t;

typedef std::map<instr::element_id, uint32_t> coverage_t;

class Coverage;

class ProgramKnowledge {
  friend class Coverage;

  typedef std::pair<instr::element_id, uint32_t> lru_cache_key_t;
  typedef instr::element_id lru_cache_value_t;

  struct hash_key_t {
    size_t operator()(const lru_cache_key_t &e) const {
      return std::hash<uint32_t>()(e.first) ^ std::hash<uint32_t>()(e.second);
    }
  };

  struct equal_to_key_t {
    bool operator()(const lru_cache_key_t &a, const lru_cache_key_t &b) const {
      return a.first == b.first && a.second == b.second;
    }
  };

  typedef cache::lru_cache<lru_cache_key_t, lru_cache_value_t, hash_key_t,
                           equal_to_key_t>
      lru_cache_t;

  bool blind = false;
  std::unique_ptr<instr::Store> store;
  std::unique_ptr<Coverage> coverage;
  std::unique_ptr<lru_cache_t> block_cache;

  // Only set when mocking models...
  std::unique_ptr<utils::Rand> random;

public:
  ProgramKnowledge() = delete;
  ProgramKnowledge(const ProgramKnowledge &) = delete;
  ProgramKnowledge &operator=(const ProgramKnowledge &) = delete;
  ~ProgramKnowledge() = default;

  ProgramKnowledge(const std::string &models_file);
  ProgramKnowledge(const bool blind);

  //
  // Store methods
  //
  instr::Store *get_store() const { return store.get(); }

  instr::lookup_t &elements() const { return store->store().elements; }

  instr::sources_t &sources() const { return store->store().sources; }

  instr::element_id get_block_element(const instr::element_id func_id,
                                      const uint32_t block_id);

  //
  // Coverage methods
  //
  const coverage_t &get_local_coverage() const;

  const score_map &get_coverage_scores() const;

  const score_map &get_goals_scores() const;

  void add_trace(const uint64_t testcase_id, shm::Container::trace_t &trace);

  trace_score_t evaluate_trace(const uint64_t testcase_id,
                               shm::Container::trace_t &trace);

  // XXX argh, that's nasty :/
  void add_trace(const uint64_t testcase_id,
                 shm::Container::mocked_trace_t &trace);

  void to_dot(const std::string &filename);

  std::pair<uint32_t, uint32_t> coverage_size();

  void reset_scores();

private:
  void initialize();
  void create_mocking_random();
};

struct GoalScoringMechanism {
  uint32_t operator()(const std::shared_ptr<instr::SummaryElement> &e) const;
};

// XXX use a more compact representation for everything
class Coverage {
  // A bundled property for our vertex is the element_id this vertex
  // points to. Vertices are basic blocks.
  struct vertex_bundle_t {
    instr::element_id element;
  };

  // In our graph, we store vertices and edges in a vector. That's actually
  // better for a more compact representation of the graph.
  typedef bgl::adjacency_list<bgl::vecS, bgl::vecS, bgl::bidirectionalS,
                              vertex_bundle_t>
      graph_t;
  typedef bgl::graph_traits<graph_t>::vertex_descriptor vertex_t;
  typedef bgl::graph_traits<graph_t>::vertex_iterator vertex_iterator;
  typedef bgl::graph_traits<graph_t>::edge_descriptor edge_t;
  typedef bgl::graph_traits<graph_t>::edge_iterator edge_iterator;

  ProgramKnowledge &knowledge;

  // XXX use a compact representation of sets of integers
  std::set<uint32_t> reached_functions;
  std::set<instr::element_id> covered_goals;
  coverage_t local_coverage;

  score_map coverage_scores;
  score_map goal_scores;
  GoalScoringMechanism goal_scoring;

  std::map<instr::element_id, score_t> mocked_scores;

  graph_t graph;

  typedef instr::element_id lru_cache_key_t;
  typedef vertex_t lru_cache_value_t;

  struct hash_key_t {
    size_t operator()(const lru_cache_key_t &e) const {
      return std::hash<uint32_t>()(e);
    }
  };

  struct equal_to_key_t {
    bool operator()(const lru_cache_key_t &a, const lru_cache_key_t &b) const {
      return a == b;
    }
  };

  typedef cache::lru_cache<lru_cache_key_t, lru_cache_value_t, hash_key_t,
                           equal_to_key_t>
      lru_cache_t;
  std::unique_ptr<lru_cache_t> vertex_cache;

public:
  Coverage() = delete;
  Coverage(const Coverage &) = delete;
  Coverage &operator=(const Coverage &) = delete;
  Coverage(ProgramKnowledge &knowledge);

  void add_trace(const uint64_t testcase_id, shm::Container::trace_t &trace);

  trace_score_t evaluate_trace(const uint64_t testcase_id,
                               shm::Container::trace_t &trace);

  // XXX argh, that's nasty :/
  void add_trace(const uint64_t testcase_id,
                 shm::Container::mocked_trace_t &trace);

  void
  add_trace_element(const uint64_t testcase_id, shm::TraceElement &e,
                    bool mock = false,
                    std::list<instr::element_id> *trace_list_ptr = nullptr);

  template <class IntSetMap> class edge_writer {
  public:
    edge_writer(const IntSetMap &s) : s(s) {}
    template <class Edge>
    void operator()(std::ostream &out, const Edge &e) const {
      out << "[label=\"";
      for (auto &i : s[e]) {
        out << i << ",";
      }
      out << "\"]";
    }

  private:
    IntSetMap s;
  };

  template <class IntSetMap>
  inline edge_writer<IntSetMap> make_edge_writer(IntSetMap s) {
    return edge_writer<IntSetMap>(s);
  }

  void to_dot(const std::string &filename);

  std::pair<uint32_t, uint32_t> size() const;

  const coverage_t &get_local_coverage() const { return local_coverage; }
  const score_map &get_coverage_scores() const { return coverage_scores; }
  const score_map &get_goals_scores() const { return goal_scores; }

  void reset_scores();

private:
  void update_local_coverage(const instr::element_id elmt);

  void update_coverage_score(const uint64_t testcase_id,
                             const uint32_t abs_score,
                             const uint32_t diff_score,
                             bool initialize = false);
  void update_goal_score(const uint64_t testcase_id, const uint32_t abs_score,
                         const uint32_t diff_score, bool initialize = false);

  void lookup_goals(const instr::element_id source,
                    const instr::element_id dest,
                    const uint64_t testcase_id = 0);

  score_t compute_goal_score(const instr::element_id elmt_id);

  score_t compute_mocked_score(const instr::element_id elmt_id);

  Coverage::vertex_t find_vertex(const instr::element_id element, bool &found);

  void add_edge(const instr::element_id source, const instr::element_id dest,
                const uint64_t testcase_id = 0, bool mock = false);

  Coverage::vertex_t add_vertex(instr::element_id vertex);
};
}

#endif
