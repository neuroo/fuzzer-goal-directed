#ifndef MOCKER_H
#define MOCKER_H

#include "shared-data/shared-data.h"
#include "utils.h"

#include <memory>

namespace fuzz {

struct Mocker {
  std::unique_ptr<utils::Rand> &random;

  Mocker() = delete;
  Mocker(const Mocker &) = delete;
  Mocker &operator=(const Mocker &) = delete;

  Mocker(std::unique_ptr<utils::Rand> &random) : random(random){};

  // Just a small utility to generate a trace without having to actually
  // execute an instrumented binary...
  shm::Container::mocked_trace_t
  generate_random_trace(const uint64_t testcase_id);
};
}

#endif
