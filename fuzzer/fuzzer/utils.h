#ifndef UTILS_H
#define UTILS_H

#include "common/elements.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "uint128_t.h"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include <boost/chrono/chrono.hpp>
#include <boost/timer/timer.hpp>

namespace utils {

// Handle the p-random number generation. This is based on boost.random
// and the mersenne twister PRNG.
struct Rand {
  // XXX a mutex on a random number generator is a stupid idea since it's
  //     called frequently. In multi-threaded environment, we'll create
  //     a new instance of `Rand` per thread
  std::mutex muastex;
  uint32_t seed;
  boost::random::mt19937 generator;
  boost::random::uniform_int_distribution<> distributor;

  Rand() = delete;
  Rand(const Rand &) = delete;
  Rand &operator=(const Rand &) = delete;
  Rand(uint32_t seed) : seed(seed) { generator.seed(seed); }

  uint32_t next_number(const uint32_t limit);
  uint32_t next_int();
  uint8_t next_char();
  char next_ascii();
  bool next_bool();

  template <typename RandomAccessIterator>
  void shuffle(RandomAccessIterator first, RandomAccessIterator last) {
    boost::variate_generator<boost::mt19937 &, boost::uniform_int<>>
        random_number_shuffler(generator, boost::uniform_int<>());
    std::random_shuffle(first, last, random_number_shuffler);
  }

  template <typename NumberType>
  std::vector<NumberType> pick(uint32_t num_elements, NumberType high) {
    std::set<NumberType> values;
    while (values.size() != num_elements) {
      values.insert(next_number(high));
    }
    return std::vector<NumberType>(values.begin(), values.end());
  }
};

std::string hex_dump(uint8_t *data, uint32_t size);

// Escape for bash strings
std::string shell_escape(const std::string &input);

std::string html_escape(const uint8_t c);

// Compute a 128bit hash based on murmur3
container::uint128_t hash(uint8_t *data, uint32_t size);

void replace_all(std::string &str, const std::string &from,
                 const std::string &to);

//
// Timer
//
struct timer_t {
  typedef boost::chrono::high_resolution_clock Clock;
  Clock::time_point start;

  timer_t() : start(Clock::now()) {}

  typename Clock::duration elapsed() const { return Clock::now() - start; }

  double seconds() const {
    return elapsed().count() *
           ((double)Clock::period::num / Clock::period::den);
  }
};
}

#endif
