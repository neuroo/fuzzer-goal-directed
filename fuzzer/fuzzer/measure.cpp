#include "measure.h"

using namespace std;

namespace fuzz {

namespace measure {

//
// score_t
//
uint64_t score_t::norm() const {
  if (absolute == 0 && diff == 0)
    return 0;
  const long double abs_square = std::pow((long double)absolute, 2);
  const long double diff_square = 9 * std::pow((long double)diff, 2);
  return (uint64_t)std::ceil(std::sqrt(((abs_square + diff_square) / 10)));
}

string score_t::toString() const {
  std::ostringstream oss;
  oss << "[abs=" << absolute << ", diff=" << diff << "]";
  return oss.str();
}

ostream &operator<<(ostream &os, const score_t &s) {
  return os << s.toString();
}

//
// measure_t
//
ostream &operator<<(ostream &os, const measure_t &s) {
  return os << s.toString();
}

string measure_t::toString() const {
  std::ostringstream oss;
  oss << "[goal=" << goal << ", edge=" << edge << ", length=" << length << "]";
  return oss.str();
}

bool measure_t::operator<(const measure_t &m) const {
  const uint64_t a_n1 = edge.norm(), a_n2 = goal.norm();
  const uint64_t b_n1 = m.edge.norm(), b_n2 = m.goal.norm();

  const long double weighted_a = 0.3 * a_n1 + 0.7 * a_n2,
                    weighted_b = 0.3 * b_n1 + 0.7 * b_n2;

  if (weighted_a == weighted_b) {
    // This is counter intuitive, but since we want to emphasize on the
    // smaller payloads, a longer one will be considered "smaller" in
    // our EV
    return length > m.length;
  }
  return weighted_a < weighted_b;
}

bool measure_t::operator>(const measure_t &m) const {
  return this->operator!=(m) && !this->operator<(m);
}

bool measure_t::operator<=(const measure_t &m) const {
  return this->operator==(m) || this->operator<(m);
}

bool measure_t::operator>=(const measure_t &m) const {
  return this->operator==(m) || this->operator>(m);
}

bool measure_t::operator==(const measure_t &m) const {
  return goal == m.goal && edge == m.edge && length == m.length;
}

//
// Different utils
//
bool greater_measure_t::operator()(const measure_t &a,
                                   const measure_t &b) const {
  return a > b;
}

bool lower_measure_t::operator()(const measure_t &a, const measure_t &b) const {
  return a < b;
}

// end namespace=measure
}
// end namespace=fuzz
}
