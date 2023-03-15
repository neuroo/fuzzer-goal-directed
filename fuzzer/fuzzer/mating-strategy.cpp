#include "mating-strategy.h"
#include "common/logger.h"
#include "evolution.h"
#include "sequence.h"
#include "utils.h"

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
namespace bm = boost::bimaps;

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
using namespace std;

namespace fuzz {

namespace ga {

typedef bm::bimap<bm::unordered_set_of<uint32_t>,
                  bm::multiset_of<measure_t, greater_measure_t>>
    score_bimap_t;
typedef score_bimap_t::value_type score_element_t;

// The elitism pairs the best performers together
index_map
ElitismMatingStrategy::apply(utils::Rand &random,
                             const index_fitness_map &fitness,
                             const individual_set &individuals) const {
  score_bimap_t index_aggregate_scores;
  const uint32_t num_individuals = individuals.size();
  for (uint32_t i = 0; i < num_individuals; i++) {
    index_aggregate_scores.insert(score_element_t(i, fitness.at(i)));
  }

  // Iteration from higher score to lower score. If one of the element
  // is not defined, we do not insert anything.
  index_map pairs;
  for (score_bimap_t::right_const_iterator
           i = index_aggregate_scores.right.begin(),
           iend = index_aggregate_scores.right.end();
       i != iend;) {
    auto first = i;
    auto second = ++i;
    if (first == iend || second == iend)
      break;
    // LOG(INFO) << "Mating: " << first->second << " with " << second->second
    //          << " (scores " << first->first << " resp. " << second->first;
    pairs.insert({first->second, second->second});
    ++i;
  }
  return pairs;
}

index_map
UniformMatingStrategy::apply(utils::Rand &random,
                             const index_fitness_map &fitness,
                             const individual_set &individuals) const {
  const uint32_t size = individuals.size();
  uint32_t num_pairs = size / 2 + size % 2;
  index_map pairs;
  for (uint32_t i = 0; i < num_pairs; i++) {
    pairs.insert({random.next_number(size), random.next_number(size)});
  }
  return pairs;
}

index_map
ClosenessMatingStrategy::apply(utils::Rand &random,
                               const index_fitness_map &fitness,
                               const individual_set &individuals) const {
  index_map pairs;

  vector<seq::pairwise_score_t> pairwise_score =
      seq::compute_pairwise_score(individuals);

  // Sort by bigger score...
  std::sort(
      begin(pairwise_score), end(pairwise_score),
      [](seq::pairwise_score_t const &t1, seq::pairwise_score_t const &t2) {
        return std::greater<size_t>()(get<2>(t1), get<2>(t2));
      });

  // Prepare the set of remaining index
  set<size_t> result;
  size_t counter = 0;
  std::generate_n(std::inserter(result, result.begin()), individuals.size(),
                  [&counter]() { return counter++; });

  for (auto &pw_score : pairwise_score) {
    const size_t idx_1 = get<0>(pw_score), idx_2 = get<1>(pw_score),
                 score = get<2>(pw_score);
    if (score < 1)
      break;
    if (idx_1 == idx_2) {
      LOG(ERROR) << "Same index in pairwise_score";
      continue;
    }
    if (result.find(idx_1) != result.end() &&
        result.find(idx_2) != result.end()) {
      // Create the matching pair
      // LOG(INFO) << "Mating: " << idx_1 << " with " << idx_2 << " (scores "
      //          << score << ")";
      pairs.insert({idx_1, idx_2});
      result.erase(idx_1);
      result.erase(idx_2);
    }
  }

  return pairs;
}

// end namespace=ga
}
// end namespace=fuzz
}
