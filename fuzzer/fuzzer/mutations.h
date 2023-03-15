#ifndef MUTATIONS_H
#define MUTATIONS_H

#include "evolution.h"
#include "utils.h"

namespace fuzz {

namespace ga {

// These are the mutations in libFuzzer
struct FlipBitMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct InsertByteMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct EraseByteMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct ChangeByteMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct SwapByteMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct ShuffleBytesMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct DuplicateByteMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct DuplicateBytesMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

struct ChangeASCIIIntegerMutator : public Mutation {
  Individual apply(utils::Rand &random, const Individual &i) const;
};

// end namespace=ga
}
// end namespace=fuzz
}

#endif
