#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "evolution.h"

#include <cstdint>
#include <set>
#include <tuple>
#include <vector>

namespace fuzz {

namespace seq {

// Returns a list of indices where the buffers b1 and b2 aren't aligned
std::set<size_t> get_not_aligned(uint8_t *source_buffer, size_t source_size,
                                 uint8_t *cmp_buffer, size_t cmp_size,
                                 bool flexible = true);

typedef std::tuple<size_t, size_t, size_t> pairwise_score_t;
std::vector<pairwise_score_t>
compute_pairwise_score(const ga::individual_set &individuals,
                       bool flexible = true);

// end namespace=seq
}
// end namespace=fuzz
}

#endif
