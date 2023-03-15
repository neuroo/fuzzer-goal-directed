#ifndef EVOLUTION_H
#define EVOLUTION_H

#include "evolution-interface.h"
#include "knowledge.h"
#include "measure.h"
#include "memory-manager.h"
#include "utils.h"

#include <boost/heap/fibonacci_heap.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
namespace bh = boost::heap;

#include "bf.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_set>
#include <vector>

namespace fuzz {

namespace ga {
typedef measure::index_map index_map;
typedef measure::measure_t measure_t;
typedef measure::greater_measure_t greater_measure_t;
typedef measure::lower_measure_t lower_measure_t;
typedef measure::index_fitness_map index_fitness_map;
typedef measure::score_map score_map;
typedef measure::index_score index_score;
typedef measure::score_t score_t;

struct Mutation;

struct Genealogy {
  std::vector<std::shared_ptr<Genealogy>> parents;
  std::vector<std::shared_ptr<Genealogy>> children;

  uint32_t generation = 0;

  Genealogy() = default;
  Genealogy(const Genealogy &g)
      : parents(g.parents), children(g.children), generation(g.generation) {}
  Genealogy &operator=(const Genealogy &g) {
    if (this != &g) {
      parents = g.parents;
      children = g.children;
      generation = g.generation;
    }
    return *this;
  }
};

struct Individual;

// A copy friendly representation of our individual
struct ShareableIndividual {
  uint64_t id = 0;
  size_t length = 0;
  uint8_t *buffer = nullptr;

  ShareableIndividual() = default;
  ShareableIndividual(const ShareableIndividual &s)
      : id(s.id), length(s.length) {
    copy_buffer(s.buffer, length);
  }
  ShareableIndividual(const Individual &);
  ShareableIndividual &operator=(const ShareableIndividual &s) {
    if (this != &s) {
      id = s.id;
      length = s.length;
      copy_buffer(s.buffer, length);
    }
    return *this;
  }
  ~ShareableIndividual();

private:
  void copy_buffer(uint8_t *src, const size_t size);
};

// The individual represents a given member of the population. We keep its
// genealogy around to get more knowledge on how it was generated. The
// individual for the genetic algorithm is a raw blob of memory. No structure
// is currently applied to it. For structured GA, we'll need to implement
// a genetic programming or grammar evolution algorithm. Ideally, we'd combine
// the two.
struct Individual {
  MemoryManager &memory;
  std::shared_ptr<Genealogy> genealogy;
  size_t index;
  uint64_t id = 0; // testcase_id

  Individual(MemoryManager &memory) : memory(memory), index(UNINIT_INDEX){};

  Individual(const Individual &i);

  ~Individual();

  Individual &operator=(const Individual &i);

  Individual clone(bool keep_id = false) const;

  void set(uint8_t *data, size_t size);

  utils::container::uint128_t hash() const;

  std::string toString() const;

  ShareableIndividual toShared() const;

  friend bool operator==(const Individual &i1, const Individual &i2) {
    if (i1.index == i2.index)
      return true;
    size_t s1 = i1.memory.length(i1.index), s2 = i1.memory.length(i2.index);
    if (s1 != s2)
      return false;
    uint8_t *b1 = i1.memory.buffer(i1.index), *b2 = i1.memory.buffer(i2.index);
    return std::memcmp(b1, b2, s1) == 0;
  }

  friend bool operator!=(const Individual &i1, const Individual &i2) {
    return !(i1 == i2);
  }

  bool operator<(const Individual &rhs) const {
    return index < rhs.index; // assume that you compare the record based on a
  }
};

//
// Containers to carry our individuals.
//
typedef std::vector<Individual> individual_set;
typedef std::vector<ShareableIndividual> shareable_individual_set;

shareable_individual_set
get_shared_individuals_from_individuals(const individual_set &);

// Base class for any mutation. A mutation algorithm has a simple interface,
// takes an individual and transforms it.
struct Mutation {
  ProgramKnowledge *knowledge = nullptr;

  Mutation(ProgramKnowledge *knowledge = nullptr) : knowledge(knowledge) {}

  void set_knowledge(ProgramKnowledge *k) { knowledge = k; }

  ProgramKnowledge *get_knowledge() { return knowledge; }

  virtual Individual apply(utils::Rand &random, const Individual &) const = 0;
};

// Storage for all our mutations
class MutationHandler {
  const po::variables_map &vm;
  std::vector<std::shared_ptr<Mutation>> mutations;
  ProgramKnowledge *knowledge = nullptr;

public:
  MutationHandler(const po::variables_map &vm);
  MutationHandler(const MutationHandler &) = delete;
  MutationHandler &operator=(const MutationHandler &) = delete;

  std::shared_ptr<Mutation> choice(utils::Rand &random) const;
};

// Base class for any cross-over. A cross over algorithm has a simple interface,
// takes two parents and generate a child.
struct CrossOver {
  ProgramKnowledge *knowledge = nullptr;

  CrossOver(ProgramKnowledge *knowledge = nullptr) : knowledge(knowledge) {}

  void set_knowledge(ProgramKnowledge *k) { knowledge = k; }

  ProgramKnowledge *get_knowledge() { return knowledge; }

  virtual Individual apply(utils::Rand &random, const Individual &i1,
                           const Individual &i2) const = 0;
};

class CrossOverHandler {
  const po::variables_map &vm;
  std::vector<std::shared_ptr<CrossOver>> cross_overs;
  ProgramKnowledge *knowledge = nullptr;

public:
  CrossOverHandler(const po::variables_map &vm);
  CrossOverHandler(const CrossOverHandler &) = delete;
  CrossOverHandler &operator=(const CrossOverHandler &) = delete;

  std::shared_ptr<CrossOver> choice(utils::Rand &random) const;
};

// fitness evaluation
struct FitnessStrategy {
  virtual index_fitness_map apply(const index_score &num_edges,
                                  const index_score &num_goals,
                                  const individual_set &individuals);
};

// base class for a mating strategy. This essentially
struct MatingStrategy {
  virtual index_map apply(utils::Rand &random, const index_fitness_map &fitness,
                          const individual_set &individuals) const = 0;
};

class MatingStrategyHandler {
  const po::variables_map &vm;
  std::vector<std::shared_ptr<MatingStrategy>> mating_strategies;
  ProgramKnowledge *knowledge = nullptr;

public:
  MatingStrategyHandler(const po::variables_map &vm);
  MatingStrategyHandler(const MatingStrategyHandler &) = delete;
  MatingStrategyHandler &operator=(const MatingStrategyHandler &) = delete;

  std::shared_ptr<MatingStrategy> choice(utils::Rand &random) const;
};

// THe overall population actively tested, that's what the GA uses
struct Population {
#define BEST_CANDIDATES_SIZE 500

  // A map based data structure that is constant size (i.e., cannot have more
  // than N elements) and keep removing the worst item in the map to insert
  // better ones.
  struct best_individuals_t {
    size_t max_size;
    std::mutex max_key_mutex;
    measure_t max_key;

    std::mutex min_key_mutex;
    measure_t min_key;

    std::set<utils::container::uint128_t> hashes;
    std::multimap<measure_t, Individual, lower_measure_t> container;

    best_individuals_t(const size_t max_size) : max_size(max_size) {}
    best_individuals_t() = delete;
    best_individuals_t(const best_individuals_t &b)
        : max_size(b.max_size), max_key(b.max_key), min_key(b.min_key),
          hashes(b.hashes), container(b.container) {}

    best_individuals_t &operator=(const best_individuals_t &b) {
      if (&b != this) {
        max_size = b.max_size;
        max_key = b.max_key;
        min_key = b.min_key;
        hashes = b.hashes;
        container = b.container;
      }
      return *this;
    }

    // Returns true if we inserted the new best overall.
    bool insert(const measure_t &score, const Individual &i);

    void set_min_key(const measure_t &new_min_key);
    void set_max_key(const measure_t &new_max_key);

    // Get the `n`-bests individuals. If `n` cannot be fetched from the current
    // set, we set `remaining` to the number of elements that will be needed
    // to complete to `n`
    individual_set get_best(const size_t num, size_t &remaining) const;

    std::set<uint64_t> get_indices() const;

    std::string toString() const;

  private:
    void consolidate_min_key();
  };

  individual_set seeds;
  best_individuals_t best_individuals;

  // THese are copies of the current population for the UI. It's
  // synchronized after each evolution
  std::mutex best_individuals_ro_mutex;
  struct best_shareable_individuals_t {
    measure_t max_key;
    measure_t min_key;
    shareable_individual_set individuals;
    std::multimap<measure_t, ShareableIndividual, lower_measure_t>
        best_individuals;
  };
  best_shareable_individuals_t best_individuals_ro;

  std::mutex individuals_mutex;
  individual_set individuals;

  Population() = default;
  Population(const individual_set &seeds)
      : seeds(seeds), best_individuals(BEST_CANDIDATES_SIZE) {}
  Population(const Population &p)
      : seeds(p.seeds), best_individuals(p.best_individuals),
        individuals(p.individuals) {}
  Population &operator=(const Population &p) {
    if (&p != this) {
      seeds = p.seeds;
      best_individuals = p.best_individuals;
      individuals = p.individuals;
    }
    return *this;
  }

  uint32_t size() const { return individuals.size(); }

  void inject_seeds(utils::Rand &random, uint32_t number_seeds);

  shareable_individual_set copy();

  std::map<std::string, float> get_current_stats();

  std::vector<std::pair<measure_t, ShareableIndividual>>
  get_best_ro(const size_t num);

  void assign_best_ro();

  measure_t get_min_key_ro();
  measure_t get_max_key_ro();

  void drop(utils::Rand &random, uint32_t number_individuals);

#undef BEST_CANDIDATES_SIZE
};

// This is the implementation of the genetic algorithm
class Evolver {
  const po::variables_map &vm;

  size_t median_population_size;
  size_t double_deviation_population_size;
  std::pair<size_t, size_t> population_range;

  Genealogy root;

  MemoryManager &memory;
  std::unique_ptr<Population> &population;
  std::unique_ptr<utils::Rand> &random;

  FitnessStrategy fitness_strategy;
  CrossOverHandler cross_overs;
  MutationHandler mutations;
  MatingStrategyHandler mating_strategies;

  uint32_t maximum_constant_score_iterations = 0;
  uint32_t constant_score_iterations = 0;

public:
  uint32_t generations = 0;

  Evolver() = delete;
  Evolver(const po::variables_map &vm, size_t median_population_size,
          std::pair<size_t, size_t> &population_range, MemoryManager &memory,
          std::unique_ptr<Population> &population,
          std::unique_ptr<utils::Rand> &random)
      : vm(vm), median_population_size(median_population_size),
        population_range(population_range), memory(memory),
        population(population), random(random), cross_overs(vm), mutations(vm),
        mating_strategies(vm) {
    maximum_constant_score_iterations =
        vm["max-evolution-fixpoint"].as<uint32_t>();
    double_deviation_population_size =
        population_range.second - population_range.first;
  }
  Evolver(const Evolver &) = delete;
  Evolver &operator=(const Evolver &) = delete;

  void evolve(std::unique_ptr<ProgramKnowledge> &k);

  index_map find_mates(const index_fitness_map &fitness) const;

  std::vector<size_t>
  get_best_performers(const size_t num_individuals,
                      const index_fitness_map &fitness) const;

  Individual get_and_mutate(const size_t index, utils::Rand &rand_ref);
  Individual get_and_mutate(const Individual &cur_ind, utils::Rand &rand_ref);

  void gather_individual_scores(const std::unique_ptr<ProgramKnowledge> &k,
                                index_score &individual_score,
                                index_score &reached_goals);

private:
  std::set<size_t> get_active_individual_indices() const;

  void global_perturbation(utils::Rand &rand_ref);
};
}
}

#endif
