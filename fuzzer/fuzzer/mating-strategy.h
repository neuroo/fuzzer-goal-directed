#ifndef MATING_STRATEGY_H
#define MATING_STRATEGY_H

#include "evolution.h"
#include "measure.h"
#include "utils.h"

namespace fuzz {

namespace ga {
typedef measure::index_fitness_map index_fitness_map;

struct ElitismMatingStrategy : public MatingStrategy {
  index_map apply(utils::Rand &random, const index_fitness_map &fitness,
                  const individual_set &individuals) const;
};

// These are the mutations in libFuzzer
struct UniformMatingStrategy : public MatingStrategy {
  index_map apply(utils::Rand &random, const index_fitness_map &fitness,
                  const individual_set &individuals) const;
};

struct ClosenessMatingStrategy : public MatingStrategy {
  index_map apply(utils::Rand &random, const index_fitness_map &fitness,
                  const individual_set &individuals) const;
};

// end namespace=ga
}
// end namespace=fuzz
}

#endif
