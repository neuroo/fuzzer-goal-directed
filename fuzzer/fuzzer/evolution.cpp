#include "evolution.h"
#include "common/logger.h"
#include "cross-overs.h"
#include "mating-strategy.h"
#include "mutations.h"
#include "utils.h"

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
namespace bm = boost::bimaps;

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
using namespace std;

namespace fuzz {

namespace ga {

#define PRINT_MUTATION_CONTENTS 0
#define DEBUG_MUTATION_CONTENTS 0
#define DEBUG_CROSS_OVER_CONTENTS 0
#define MUTATION_FIXPOINT_MAX_RETRY 5
#define SEED_INJECTION_NUMBER 10
#define CROSS_OVER_CHILD_GROWTH 2

//
// ShareableIndividual
//
ShareableIndividual::ShareableIndividual(const Individual &ind) {
  id = ind.id;
  length = ind.memory.length(ind.index);
  copy_buffer(ind.memory.buffer(ind.index), length);
}

ShareableIndividual::~ShareableIndividual() {
  if (buffer) {
    delete[] buffer;
  }
}

void ShareableIndividual::copy_buffer(uint8_t *src, const size_t size) {
  if (size < 1)
    return;
  if (buffer != nullptr)
    delete[] buffer;
  buffer = new (std::nothrow) uint8_t[size];
  std::memcpy(buffer, src, size);
}

//
// Individual
//
Individual::Individual(const Individual &i)
    : memory(i.memory), genealogy(i.genealogy), index(i.index), id(i.id) {
  i.memory.increase_count(i.index);
}

Individual::~Individual() { memory.decrease_count(index); }

Individual &Individual::operator=(const Individual &i) {
  if (this != &i) {
    memory = i.memory;
    genealogy = i.genealogy;
    index = i.index;
    id = i.id;
    memory.increase_count(index);
  }
  return *this;
}

Individual Individual::clone(bool keep_id) const {
  Individual i(*this);
  if (!keep_id) {
    // The usual cloning mechanism is called for creating a new individual
    // so we will create new memory slot and reset the testcase id (`id`)
    size_t new_index = memory.copy(i.index);
    i.index = new_index;
    i.id = 0;
  }
  return i;
}

void Individual::set(uint8_t *data, size_t size) {
  MemoryManager::triplet_t t = memory.create(size);
  index = t.index;
  std::memcpy(t.ptr, data, size);
}

utils::container::uint128_t Individual::hash() const {
  return utils::hash(memory.buffer(index), memory.length(index));
}

ShareableIndividual Individual::toShared() const {
  return ShareableIndividual(*this);
}

string Individual::toString() const {
  ostringstream oss;
  oss << "<Individual memory_slot=" << index << " id=" << id
      << " size=" << memory.length(index)
#if (PRINT_MUTATION_CONTENTS == 1)
      << " hash=" << std::hex
      << utils::hash(memory.buffer(index), memory.length(index)) << std::dec
      << " data=\n"
      << utils::hex_dump(memory.buffer(index), memory.length(index))
#endif
      << ">";
  return oss.str();
}

//
// Containers to carry our individuals
//
shareable_individual_set
get_shared_individuals_from_individuals(const individual_set &inds) {
  shareable_individual_set sinds(inds.size());
  for (auto &ind : inds) {
    sinds.push_back(ind.toShared());
  }
  return sinds;
}

//
// Mutations
//
MutationHandler::MutationHandler(const po::variables_map &vm) : vm(vm) {
  // Instantiate all our mutators
  mutations.push_back(shared_ptr<Mutation>(new FlipBitMutator()));
  mutations.push_back(shared_ptr<Mutation>(new InsertByteMutator()));
  mutations.push_back(shared_ptr<Mutation>(new EraseByteMutator()));
  mutations.push_back(shared_ptr<Mutation>(new ChangeByteMutator()));
  mutations.push_back(shared_ptr<Mutation>(new SwapByteMutator()));
  mutations.push_back(shared_ptr<Mutation>(new ShuffleBytesMutator()));
  mutations.push_back(shared_ptr<Mutation>(new DuplicateByteMutator()));
  mutations.push_back(shared_ptr<Mutation>(new DuplicateBytesMutator()));
  mutations.push_back(shared_ptr<Mutation>(new ChangeASCIIIntegerMutator()));
}

shared_ptr<Mutation> MutationHandler::choice(utils::Rand &random) const {
  uint16_t idx = random.next_number(mutations.size());
  return mutations[idx];
}

//
// Cross Over
//
CrossOverHandler::CrossOverHandler(const po::variables_map &vm) : vm(vm) {
  // Instantiate all our cross-over operators
  cross_overs.push_back(shared_ptr<CrossOver>(new SinglePointCrossOver()));
  cross_overs.push_back(shared_ptr<CrossOver>(new NPointsCrossOver()));
  cross_overs.push_back(shared_ptr<CrossOver>(new UniformCrossOver()));
  // cross_overs.push_back(shared_ptr<CrossOver>(new AlignmentCrossOver()));
}

shared_ptr<CrossOver> CrossOverHandler::choice(utils::Rand &random) const {
  uint16_t idx = random.next_number(cross_overs.size());
  return cross_overs[idx];
}

//
// Mating strategies
//
MatingStrategyHandler::MatingStrategyHandler(const po::variables_map &vm)
    : vm(vm) {
  mating_strategies.push_back(
      shared_ptr<MatingStrategy>(new ElitismMatingStrategy()));
  mating_strategies.push_back(
      shared_ptr<MatingStrategy>(new UniformMatingStrategy()));

  if (vm["slow-mating-strategies"].as<bool>()) {
    LOG(INFO) << "Enable slow mating strategy: ClosenessMatingStrategy";
    mating_strategies.push_back(
        shared_ptr<MatingStrategy>(new ClosenessMatingStrategy()));
  }
}

shared_ptr<MatingStrategy>
MatingStrategyHandler::choice(utils::Rand &random) const {
  uint16_t idx = random.next_number(mating_strategies.size());
  return mating_strategies[idx];
}

//
// FitnessStrategy
//
index_fitness_map FitnessStrategy::apply(const index_score &num_edges,
                                         const index_score &num_goals,
                                         const individual_set &individuals) {
  index_fitness_map fitness;
  const uint32_t num_individuals = individuals.size();
  for (uint32_t i = 0; i < num_individuals; i++) {
    score_t edges;
    score_t goals;
    const Individual &ind = individuals[i];
    const size_t length = ind.memory.length(ind.index);

    auto edge_score_iter = num_edges.find(i);
    if (edge_score_iter != num_edges.end()) {
      edges = edge_score_iter->second;
    }

    auto goal_score_iter = num_goals.find(i);
    if (goal_score_iter != num_goals.end()) {
      goals = goal_score_iter->second;
    }

    fitness.insert({i, measure_t(edges, goals, length)});
  }
  return fitness;
}

//
// Population methods
//
bool Population::best_individuals_t::insert(const measure_t &score,
                                            const Individual &i) {
  using namespace utils::container;
  uint128_t hash_value = i.hash();
  if (hashes.find(hash_value) != hashes.end())
    return false;

  // Did we insert a best?
  bool result = false;

  if (container.size() >= max_size) {
    if (score < min_key) {
      // Skip a possible worst candidate
      return false;
    }
    // We need to find the first worst candidate to remove. We get it by
    // removing one from min_key
    auto min_iter = container.find(min_key);
    if (min_iter != container.end()) {
      Individual min_ind = min_iter->second;
      uint128_t min_hash_value = min_ind.hash();
      container.erase(min_iter);
      hashes.erase(min_hash_value);
    } else {
      LOG(ERROR) << "Cannot find a candidate with min_key=" << min_key;
    }

    if (score > max_key) {
      set_max_key(score);
      result = true;
    }

    Individual ind = i.clone(/*keep_id*/ true);
    container.insert({score, ind});
    hashes.insert(hash_value);
    consolidate_min_key();
  } else {
    // Free for all...
    if (score < min_key)
      set_min_key(score);
    else if (score > max_key) {
      result = true;
      set_max_key(score);
    }
    hashes.insert(hash_value);
    container.insert({score, i.clone(/*keep_id*/ true)});
  }
  return result;
}

void Population::best_individuals_t::set_min_key(const measure_t &new_min_key) {
  std::lock_guard<std::mutex> lock(min_key_mutex);
  min_key = new_min_key;
}

void Population::best_individuals_t::set_max_key(const measure_t &new_max_key) {
  std::lock_guard<std::mutex> lock(max_key_mutex);
  max_key = new_max_key;
}

// XXX Ideally, we also want to favorite smaller inputs...
individual_set
Population::best_individuals_t::get_best(const size_t num,
                                         size_t &remaining) const {
  vector<Individual> result;
  size_t current = 0;
  size_t num_inds = num;

  if (num > container.size()) {
    remaining = num_inds - container.size();
    num_inds = container.size();
  }

  for (auto iter = container.rbegin(); iter != container.rend(); ++iter) {
    result.push_back(iter->second.clone(/*keep_id*/ true));
    current++;
    if (current >= num_inds)
      break;
  }
  return result;
}

set<uint64_t> Population::best_individuals_t::get_indices() const {
  set<uint64_t> indices;
  for (auto iter = container.begin(); iter != container.end(); ++iter) {
    indices.insert(iter->second.index);
  }
  return indices;
}

string Population::best_individuals_t::toString() const {
  ostringstream oss;
  oss << "<Best Individuals ";
  for (auto iter = container.begin(); iter != container.end(); ++iter) {
    oss << endl << iter->first << " ~~> " << iter->second.toString();
  }
  return oss.str();
}

void Population::best_individuals_t::consolidate_min_key() {
  set_min_key(measure_t(score_t(UINT32_MAX, UINT32_MAX),
                        score_t(UINT32_MAX, UINT32_MAX), UINT32_MAX));
  for (auto iter = container.begin(); iter != container.end(); ++iter) {
    if (iter->first < min_key)
      set_min_key(iter->first);
  }
}

void Population::inject_seeds(utils::Rand &random, uint32_t number_seeds) {
  // We take at most 25% of the original seeds
  vector<uint32_t> selected = random.pick(number_seeds, (uint32_t)seeds.size());
  for (auto &i : selected) {
    individuals.push_back(seeds[i]);
  }
}

void Population::drop(utils::Rand &random, uint32_t number_individuals) {
  vector<uint32_t> selected =
      random.pick(number_individuals, (uint32_t)individuals.size());
  LOG(INFO) << "Drop: " << selected << " (size=" << individuals.size() << ")";
  std::sort(selected.begin(), selected.end(), std::greater<uint32_t>());

  for (auto &i : selected) {
    individuals.erase(individuals.begin() + i);
  }
}

shareable_individual_set Population::copy() {
  return get_shared_individuals_from_individuals(individuals);
}

measure_t Population::get_min_key_ro() {
  std::lock_guard<std::mutex> lock(best_individuals_ro_mutex);
  return best_individuals_ro.min_key;
}

measure_t Population::get_max_key_ro() {
  std::lock_guard<std::mutex> lock(best_individuals_ro_mutex);
  return best_individuals_ro.max_key;
}

map<string, float> Population::get_current_stats() {
  std::lock_guard<std::mutex> lock(best_individuals_ro_mutex);
  if (best_individuals_ro.individuals.empty()) {
    return map<string, float>();
  }

  auto letter_density = [](uint8_t *buffer, size_t length) -> float {
    float density = 0.0;
    for (size_t i = 0; i < length; i++) {
      if (std::isalpha(buffer[i])) {
        density += 1.0;
      }
    }
    return density / (float)length;
  };

  auto digit_density = [](uint8_t *buffer, size_t length) -> float {
    float density = 0.0;
    for (size_t i = 0; i < length; i++) {
      if (std::isdigit(buffer[i])) {
        density += 1.0;
      }
    }
    return density / (float)length;
  };

  map<string, float> stats;
  float average_size = 0.0;
  float average_letter_density = 0.0;
  float average_digit_density = 0.0;

  float size = 0.0;
  for (auto &x : best_individuals_ro.individuals) {
    const size_t length = x.length;
    uint8_t *buffer = x.buffer;

    if (buffer != nullptr) {
      size += 1.0;
      average_size += (float)length;
      average_letter_density += letter_density(buffer, length);
      average_digit_density += digit_density(buffer, length);
    }
  }

  if (size > 0.0) {
    average_size /= size;
    average_letter_density /= size;
    average_digit_density /= size;

    stats.insert({"Average Size", average_size});
    stats.insert({"Average Letter Density", average_letter_density});
    stats.insert({"Average Digit Density", average_digit_density});
  }

  return stats;
}

void Population::assign_best_ro() {
  std::lock_guard<std::mutex> lock(best_individuals_ro_mutex);
  {
    std::lock_guard<std::mutex> lock(best_individuals.max_key_mutex);
    best_individuals_ro.max_key = best_individuals.max_key;
  }
  {
    std::lock_guard<std::mutex> lock(best_individuals.min_key_mutex);
    best_individuals_ro.min_key = best_individuals.min_key;
  }
  for (auto &x : best_individuals.container) {
    best_individuals_ro.best_individuals.insert({x.first, x.second.toShared()});
  }
  best_individuals_ro.individuals = copy();
}

vector<pair<measure_t, ShareableIndividual>>
Population::get_best_ro(const size_t num) {
  std::lock_guard<std::mutex> lock(best_individuals_ro_mutex);

  vector<pair<measure_t, ShareableIndividual>> bests;
  size_t num_best = std::min(num, best_individuals_ro.best_individuals.size());
  for (auto iter = best_individuals_ro.best_individuals.rbegin();
       iter != best_individuals_ro.best_individuals.rend(); ++iter) {
    bests.push_back(make_pair(iter->first, iter->second));
    if (--num_best < 1)
      break;
  }
  return bests;
}

//
// Evolver methods
//

// This is the core of the genetic algorithm. It performs one iteration.
void Evolver::evolve(unique_ptr<ProgramKnowledge> &k) {
  ++generations;
  ++constant_score_iterations;

  utils::Rand &rand_ref = *random;
  bool improved = false;

  LOG(INFO) << "Proceeding with " << generations << "th generation";
  LOG(INFO) << "Current population size: " << population->size();

  // 0. Acquire performance for each test case.
  index_score individual_scores; // index -> score
  index_score reached_goals;     // index -> score
  gather_individual_scores(k, individual_scores, reached_goals);

  const index_fitness_map fitness = fitness_strategy.apply(
      individual_scores, reached_goals, population->individuals);

  // LOG(INFO) << "fitness map: " << fitness;

  // 0. Register the current items with their score in the best individuals
  //    overall
  for (auto fitness_iter = fitness.begin(); fitness_iter != fitness.end();
       ++fitness_iter) {
    if (population->best_individuals.insert(
            fitness_iter->second,
            population->individuals[fitness_iter->first])) {
      improved = true;
    }
  }

  if (!improved &&
      constant_score_iterations > maximum_constant_score_iterations) {
    constant_score_iterations = 0;
    global_perturbation(rand_ref);
    memory.force_clean(get_active_individual_indices());
    return;
  }

  // 1. Perform cross-over on each tuple of selected parents (custom
  //    selection with different heuristics)
  index_map mates = find_mates(fitness);
  // LOG(INFO) << "Mating resulted in: " << mates;
  individual_set new_individuals;

  // Using a single cross-over per generation, but random mutation per new
  // individual with a given probability. The cross over might generate the
  // same child.
  shared_ptr<CrossOver> cross_over = cross_overs.choice(rand_ref);
  for (auto &m_elmt : mates) {
    shared_ptr<Mutation> mutation = mutations.choice(rand_ref);

    Individual new_member =
        cross_over->apply(rand_ref, population->individuals[m_elmt.first],
                          population->individuals[m_elmt.second]);

#if (DEBUG_CROSS_OVER_CONTENTS == 1)
    LOG(INFO) << "Parent 1:"
              << population->individuals[m_elmt.first].toString();
    LOG(INFO) << "Parent 2:"
              << population->individuals[m_elmt.second].toString();
    LOG(INFO) << "Child:" << new_member.toString();
#endif

    Individual mutated_new_member = get_and_mutate(new_member, rand_ref);

#if (DEBUG_MUTATION_CONTENTS == 1)
    LOG(INFO) << "  Before mutation:" << new_member.toString();
    LOG(INFO) << "  After mutation:" << mutated_new_member.toString();
#endif

    new_individuals.push_back(mutated_new_member);
  }

  const size_t current_pop_size = new_individuals.size();
  size_t last_injected_performer = 0;
  vector<size_t> best_performers =
      get_best_performers(current_pop_size, fitness);

  if (current_pop_size < population_range.first) {
    // Reinject the best performers and apply a random mutation to them. We go
    // up to the minimal size of our population.
    size_t injected_best_perfs = population_range.first - current_pop_size;

    LOG(INFO) << "Need to reinject " << injected_best_perfs
              << " best performers to achieve min(population_range) since the "
                 "cross-over generated "
              << current_pop_size << " individuals";

    for (size_t i = 0; i < injected_best_perfs; i++) {
      new_individuals.push_back(get_and_mutate(i, rand_ref));
      last_injected_performer = i;
    }
  }

  // Adjust the population size to be between `population_range`. This does
  // a global permutation by injecting some of our mutated overall bests.
  if (rand_ref.next_bool()) {
    uint32_t pop_size_perturbation =
        random->next_number(double_deviation_population_size) + 1;
    LOG(INFO) << "Some more reinjection, for " << pop_size_perturbation
              << " individuals from overall bests";

    size_t remaining = 0;
    vector<Individual> n_bests =
        population->best_individuals.get_best(pop_size_perturbation, remaining);
    for (auto &ind : n_bests) {
      // LOG(INFO) << "Using global best: " << ind.toString();
      new_individuals.push_back(get_and_mutate(ind, rand_ref));
    }

    if (remaining > 0) {
      // Adjust with some of the best from previous run
      for (size_t i = last_injected_performer;
           i < last_injected_performer + remaining; i++) {
        new_individuals.push_back(get_and_mutate(i, rand_ref));
      }
    }
  }

  population->individuals.assign(new_individuals.begin(),
                                 new_individuals.end());
  LOG(INFO) << "New population size: " << population->size();

  // XXX well, with a decent ref-counting system we wouldn't need that.
  memory.force_clean(get_active_individual_indices());
}

void Evolver::gather_individual_scores(const unique_ptr<ProgramKnowledge> &k,
                                       index_score &individual_scores,
                                       index_score &reached_goals) {
  const score_map &cov_coverages_scores = k->get_coverage_scores(),
                  &cov_goals_scores = k->get_goals_scores();

  auto cov_iter_scores_end = cov_coverages_scores.end();
  auto goal_iter_scores_end = cov_goals_scores.end();

  for (uint32_t i = 0; i < population->size(); i++) {
    Individual &ind = population->individuals[i];
    auto cov_iter = cov_coverages_scores.find(ind.id);
    if (cov_iter != cov_iter_scores_end) {
      if (cov_iter->second.norm() > 0) {
        individual_scores.insert({i, cov_iter->second});
      }
    }

    auto goal_iter = cov_goals_scores.find(ind.id);
    if (goal_iter != goal_iter_scores_end) {
      if (goal_iter->second.norm() > 0) {
        reached_goals.insert({i, goal_iter->second});
      }
    }
  }
}

Individual Evolver::get_and_mutate(const size_t index, utils::Rand &rand_ref) {
  Individual cur_ind = population->individuals[index];
  return get_and_mutate(cur_ind, rand_ref);
}

Individual Evolver::get_and_mutate(const Individual &cur_ind,
                                   utils::Rand &rand_ref) {
  if (rand_ref.next_number(5)) // 20% chance of mutation?!
    return cur_ind.clone();

  // Don't nullify the output, that's our only constraint...
  uint16_t tries = 0;
  while (true) {
    shared_ptr<Mutation> mutation = mutations.choice(rand_ref);
    Individual new_member = mutation->apply(rand_ref, cur_ind);
    if (new_member.memory.length(new_member.index) > 0) {
      return new_member;
    }
    ++tries;
  }
}

vector<size_t>
Evolver::get_best_performers(const size_t num_individuals,
                             const index_fitness_map &fitness) const {
  typedef bm::bimap<bm::unordered_set_of<uint32_t>,
                    bm::multiset_of<measure_t, greater_measure_t>>
      score_bimap_t;
  typedef score_bimap_t::value_type score_element_t;

  if (num_individuals < 1) {
    return vector<size_t>();
  }

  score_bimap_t index_fitness_scores;
  const uint32_t num_overall_individuals = fitness.size();
  for (uint32_t i = 0; i < num_overall_individuals; i++) {
    index_fitness_scores.insert(score_element_t(i, fitness.at(i)));
  }

  size_t inserted = 0;
  vector<size_t> performers;
  for (score_bimap_t::right_const_iterator
           i = index_fitness_scores.right.begin(),
           iend = index_fitness_scores.right.end();
       i != iend; ++i) {
    if (inserted >= num_individuals)
      break;
    performers.push_back(i->second);
  }
  return performers;
}

index_map Evolver::find_mates(const index_fitness_map &fitness) const {
  shared_ptr<MatingStrategy> mating = mating_strategies.choice(*random);
  return mating->apply(*random, fitness, population->individuals);
}

set<size_t> Evolver::get_active_individual_indices() const {
  set<size_t> indices;
  // The seeds are always active
  for (auto &ind : population->seeds) {
    indices.insert(ind.index);
  }

  for (auto &ind : population->individuals) {
    indices.insert(ind.index);
  }

  for (auto &ind : population->best_individuals.get_indices()) {
    indices.insert(ind);
  }
  return indices;
}

void Evolver::global_perturbation(utils::Rand &rand_ref) {
  LOG(INFO) << "Apply global perturbation";

  const size_t current_pop_size = population->size(),
               half_size = current_pop_size >> 1;
  size_t remaining = 0;
  individual_set starting_individuals =
      population->best_individuals.get_best(half_size, remaining);
  const size_t best_ind_pop_size = starting_individuals.size();

  // Add orignal seeds too
  for (size_t i = 0; i < half_size; i++) {
    starting_individuals.push_back(population->seeds[i]);
  }

  individual_set new_individuals;

  for (auto &ind : starting_individuals) {
    new_individuals.push_back(get_and_mutate(ind, rand_ref));
  }

  while (new_individuals.size() < current_pop_size) {
    // Randomly select 2 elements in the best_individuals and cross them over
    shared_ptr<CrossOver> cross_over = cross_overs.choice(rand_ref);
    size_t idx1 = rand_ref.next_number(best_ind_pop_size), idx2 = idx1;
    while (idx1 == idx2) {
      idx2 = rand_ref.next_number(best_ind_pop_size);
    }

    Individual new_member = cross_over->apply(
        rand_ref, population->individuals[idx1], population->individuals[idx2]);
    new_individuals.push_back(get_and_mutate(new_member, rand_ref));
  }

  // Overwrite the population
  population->individuals.assign(new_individuals.begin(),
                                 new_individuals.end());
}

#undef DEBUG_MUTATION_CONTENTS
#undef DEBUG_CROSS_OVER_CONTENTS
#undef MUTATION_FIXPOINT_MAX_RETRY
#undef SEED_INJECTION_NUMBER
#undef CROSS_OVER_CHILD_GROWTH_MULTIPLIER

// end namespace=ga
}
// end namespace=fuzz
}
