#ifndef SEEDS_H
#define SEEDS_H

#include "evolution.h"
#include "memory-manager.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace fuzz {

enum SeedFileKind { E_SEED_STRING = 1, E_SEED_FILE, E_SEED_UNKNOWN = 0xff };

struct SeedHandler {
  MemoryManager &memory;
  ga::individual_set values;

  SeedHandler() = delete;
  SeedHandler(MemoryManager &memory) : memory(memory){};

  bool parse(const std::string &seed_file);

private:
  ga::Individual create(const std::string &line, bool &errored);
};
}

#endif
