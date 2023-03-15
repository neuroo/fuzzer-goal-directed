#ifndef MEASURE_H
#define MEASURE_H

#include "common/elements.h"
#include "utils.h"

#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fuzz {

namespace measure {

// A score_t represents a local score computed for an individual. The
// first element is the absolute score, while the second is the global
// differential (over all runs, so the global improvement). Not using
// a std::pair<> to make it more explicit in the rest of the code
// we're dealing with scores.
struct score_t {
  uint32_t absolute;
  uint32_t diff;

  score_t() : absolute(0), diff(0) {}
  score_t(const uint32_t absolute, const uint32_t diff)
      : absolute(absolute), diff(diff) {}
  score_t(const score_t &s) : absolute(s.absolute), diff(s.diff) {}
  score_t &operator=(const score_t &s) {
    if (&s != this) {
      absolute = s.absolute;
      diff = s.diff;
    }
    return *this;
  }
  ~score_t() = default;

  // Just a L2 norm...
  uint64_t norm() const;

  bool operator==(const score_t &s) const {
    return absolute == s.absolute && diff == s.diff;
  }
  bool operator!=(const score_t &s) const { return !(operator==(s)); }
  bool operator<(const score_t &s) const { return norm() < s.norm(); }
  bool operator>(const score_t &s) const { return norm() > s.norm(); }
  bool operator>=(const score_t &s) const { return !(this->operator<(s)); }
  bool operator<=(const score_t &s) const { return !(this->operator>(s)); }

  std::string toString() const;
};

std::ostream &operator<<(std::ostream &os, const score_t &s);

// A measure_t represents the aggregation of all component for the final
// scoring system. At the time of the writing, the goals and coverage (edge)
// are used in combination with the length of the input.
struct measure_t {
  score_t goal;
  score_t edge;
  size_t length;

  measure_t() = default;
  measure_t(const score_t &goal, const score_t &edge, const size_t length)
      : goal(goal), edge(edge), length(length) {}
  measure_t(const measure_t &m)
      : goal(m.goal), edge(m.edge), length(m.length) {}

  measure_t &operator=(const measure_t &m) {
    if (&m != this) {
      goal = m.goal;
      edge = m.edge;
      length = m.length;
    }
    return *this;
  }

  // Just a L2,1 norm...
  uint64_t norm() const;

  bool operator==(const measure_t &m) const;
  bool operator!=(const measure_t &m) const { return !(operator==(m)); }
  bool operator<(const measure_t &m) const;
  bool operator>(const measure_t &m) const;
  bool operator>=(const measure_t &m) const;
  bool operator<=(const measure_t &m) const;

  std::string toString() const;
};

typedef std::map<uint32_t, uint32_t> index_map;
typedef std::map<uint32_t, score_t> index_score;
typedef std::map<uint32_t, measure_t> index_fitness_map;
typedef std::map<uint64_t, score_t> score_map;
typedef std::pair<std::list<instr::element_id>, measure_t> trace_score_t;

//
// Operators to work with the measure_t
//
std::ostream &operator<<(std::ostream &os, const measure_t &s);

struct greater_measure_t {
  bool operator()(const measure_t &a, const measure_t &b) const;
};

struct lower_measure_t {
  bool operator()(const measure_t &a, const measure_t &b) const;
};

// end namespace=measure
}
// end namespace=fuzz
}

#endif
