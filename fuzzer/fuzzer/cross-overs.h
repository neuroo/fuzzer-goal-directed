#ifndef CROSS_OVERS_H
#define CROSS_OVERS_H

#include "evolution.h"
#include "utils.h"

namespace fuzz {

namespace ga {

struct SinglePointCrossOver : public CrossOver {
  Individual apply(utils::Rand &random, const Individual &i1,
                   const Individual &i2) const;
};

struct NPointsCrossOver : public CrossOver {
  Individual apply(utils::Rand &random, const Individual &i1,
                   const Individual &i2) const;
};

struct UniformCrossOver : public CrossOver {
  Individual apply(utils::Rand &random, const Individual &i1,
                   const Individual &i2) const;
};

struct AlignmentCrossOver : public CrossOver {
  Individual apply(utils::Rand &random, const Individual &i1,
                   const Individual &i2) const;
};

// end namespace=ga
}
// end namespace=fuzz
}

#endif
