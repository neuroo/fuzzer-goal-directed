#include "cross-overs.h"
#include "common/logger.h"
#include "evolution.h"
#include "sequence.h"
#include "utils.h"

#include <boost/random/shuffle_order.hpp>
namespace rnd = boost::random;

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
using namespace std;

namespace fuzz {

namespace ga {

// Randomly select a point in the dest buffer. Fill either left or right with
// data from i1 or i2 (resp.)
Individual SinglePointCrossOver::apply(utils::Rand &random,
                                       const Individual &i1,
                                       const Individual &i2) const {
  Individual c = random.next_bool() ? i1.clone() : i2.clone();
  const Individual *first = nullptr, *second = nullptr;

  if (random.next_bool()) {
    first = &i1;
    second = &i2;
  } else {
    first = &i2;
    second = &i1;
  }

  size_t size = c.memory.length(c.index),
         i1_size = c.memory.length(first->index),
         i2_size = c.memory.length(second->index);

  assert(size <= std::max(i1_size, i2_size) &&
         size >= std::min(i1_size, i2_size));

  uint8_t *buffer = c.memory.buffer(c.index),
          *i1_buffer = c.memory.buffer(first->index),
          *i2_buffer = c.memory.buffer(second->index);

  uint32_t idx = size > 1 ? random.next_number(size) + 1 : 0;

  for (uint32_t j = 0; j < size; j++) {
    buffer[j] = j < idx ? (j < i1_size ? i1_buffer[j] : random.next_char())
                        : (j < i2_size ? i2_buffer[j] : random.next_char());
  }
  return c;
}

// Randomly chose N points. The dest buffer is then cut into N segments.
// Randomly select which of the parents a given segment is filled with.
Individual NPointsCrossOver::apply(utils::Rand &random, const Individual &i1,
                                   const Individual &i2) const {
#define MIN_SEGMENT_SIZE ((size_t)4)
#define NUMBER_SEGMENTS ((size_t)32)
  Individual c = random.next_bool() ? i1.clone() : i2.clone();
  size_t size = c.memory.length(c.index);

  const size_t segment_size =
      std::max((size_t)random.next_number(
                   std::max(MIN_SEGMENT_SIZE, size / NUMBER_SEGMENTS)) +
                   1,
               MIN_SEGMENT_SIZE);

  // LOG(INFO) << "Segment size: " << segment_size;
  if (segment_size > size) {
    return c;
  }

  vector<size_t> indices = random.pick(size / segment_size, size);
  if (indices.empty())
    return c;

  // LOG(INFO) << "NPoints: " << indices << " (num=" << indices.size() << ")";

  uint8_t *buffer = c.memory.buffer(c.index),
          *i1_buffer = c.memory.buffer(i1.index),
          *i2_buffer = c.memory.buffer(i2.index), *cur_buffer = nullptr;

  const Individual *src = nullptr;
  size_t finger = 0, cur_size = 0, i1_size = c.memory.length(i1.index),
         i2_size = c.memory.length(i2.index);
  for (auto &i : indices) {
    if (finger == i)
      continue;
    // Fill from finger -> i with data from randomly selected individual
    if (random.next_bool()) {
      src = &i1;
      cur_size = i1_size;
      cur_buffer = i1_buffer;
    } else {
      src = &i2;
      cur_size = i2_size;
      cur_buffer = i2_buffer;
    }

    if (i > cur_size) {
      for (size_t j = finger; j < i; j++) {
        buffer[j] = random.next_char();
      }
    } else {
      for (size_t j = finger; j < i; j++) {
        buffer[j] = cur_buffer[j];
      }
    }
    finger = i;
  }

  // The remaining of the buffer is left from the cloning

  return c;
#undef MIN_SEGMENT_SIZE
#undef NUMBER_SEGMENTS
}

// This is a typical random selection from i1 or i2 for each byte. If the byte
// doesn't exist in either, fill it with random value.
Individual UniformCrossOver::apply(utils::Rand &random, const Individual &i1,
                                   const Individual &i2) const {
  Individual c = random.next_bool() ? i1.clone() : i2.clone();

  size_t size = c.memory.length(c.index), i1_size = c.memory.length(i1.index),
         i2_size = c.memory.length(i2.index), cur_size = 0;

  assert(size <= std::max(i1_size, i2_size) &&
         size >= std::min(i1_size, i2_size));

  uint8_t *buffer = c.memory.buffer(c.index),
          *i1_buffer = c.memory.buffer(i1.index),
          *i2_buffer = c.memory.buffer(i2.index), *src = nullptr;

  for (uint32_t j = 0; j < size; j++) {
    if (random.next_bool()) {
      src = i1_buffer;
      cur_size = i1_size;
    } else {
      src = i2_buffer;
      cur_size = i2_size;
    }
    if (j < cur_size) {
      buffer[j] = src[j];
    } else {
      buffer[j] = random.next_char();
    }
  }
  return c;
}

// The smart ass cross-over tries to find commonality in i1 and i2 using
// alignment. Keep them and randomly insert bytes in non-aligned area.
Individual AlignmentCrossOver::apply(utils::Rand &random, const Individual &i1,
                                     const Individual &i2) const {
  size_t src_size = 0, cmp_size = 0, i1_size = i1.memory.length(i1.index),
         i2_size = i1.memory.length(i2.index);

  uint8_t *src_buffer = nullptr, *cmp_buffer = nullptr,
          *i1_buffer = i1.memory.buffer(i1.index),
          *i2_buffer = i1.memory.buffer(i2.index);

  const bool select = random.next_bool();
  Individual c = select ? i1.clone() : i2.clone();
  if (random.next_bool()) {
    src_size = i1_size;
    src_buffer = i1_buffer;
    cmp_buffer = i2_buffer;
    cmp_size = i2_size;
  } else {
    src_size = i2_size;
    src_buffer = i2_buffer;
    cmp_buffer = i1_buffer;
    cmp_size = i1_size;
  }

  // Retrieve the list of indices for non-aligned bytes
  set<size_t> not_aligned_indices =
      seq::get_not_aligned(src_buffer, src_size, cmp_buffer, cmp_size);

  if (not_aligned_indices.empty()) {
    // Essentially, there is no overlap between the two. So we backtrack
    // to discarding either i1 or i2 (i.e., we don't do anything.)
  } else {
    for (auto &index : not_aligned_indices) {
      if (index >= src_size) {
        LOG(ERROR) << "index shouldn't be that large";
        continue;
      }
      src_buffer[index] = random.next_char();
    }
  }
  return c;
}

// end namespace=ga
}
// end namespace=fuzz
}
