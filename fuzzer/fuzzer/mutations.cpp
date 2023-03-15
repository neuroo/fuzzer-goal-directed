#include "mutations.h"
#include "common/logger.h"
#include "evolution.h"
#include "utils.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace std;

namespace fuzz {

namespace ga {

#define MAX_INPUT_SIZE_MULTIPLIER 4
#define MAX_INPUT_SIZE_DUPLICATOR 8 // 8 bytes

// Randomly select a bit in the buffer and flip it
Individual FlipBitMutator::apply(utils::Rand &random,
                                 const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);
  uint8_t *buffer = c.memory.buffer(c.index);

  uint32_t idx = random.next_number(size);
  uint8_t bit = random.next_number(8), mask = 1 << bit, x = buffer[idx];

  buffer[idx] = (x & mask) ? x & ~mask : x | mask;
  return c;
}

// Insert a new random byte at a random location
Individual InsertByteMutator::apply(utils::Rand &random,
                                    const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);
  uint32_t idx = random.next_number(size + 1);

  c.memory.insert_bytes(c.index, idx, 1);
  assert(size + 1 == c.memory.length(c.index));

  uint8_t *buffer = c.memory.buffer(c.index);
  buffer[idx] = random.next_char();
  return c;
}

// Remove a random byte
Individual EraseByteMutator::apply(utils::Rand &random,
                                   const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);

  // Don't want an individual with no data...
  if (size < 2)
    return c;

  uint32_t idx = random.next_number(size);
  c.memory.remove_bytes(c.index, idx, 1);
  assert(size - 1 == c.memory.length(c.index));

  return c;
}

// Randomly change a value in the buffer
Individual ChangeByteMutator::apply(utils::Rand &random,
                                    const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);
  uint32_t idx = random.next_number(size);
  uint8_t *buffer = c.memory.buffer(c.index);

  buffer[idx] = random.next_char();
  return c;
}

// Swap two randomly selected bytes from the buffer
Individual SwapByteMutator::apply(utils::Rand &random,
                                  const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);
  if (size < 2)
    return c;

  uint32_t idx1 = random.next_number(size), idx2 = idx1;
  while (idx2 == idx1) {
    idx2 = random.next_number(size);
  }
  uint8_t *buffer = c.memory.buffer(c.index);
  uint8_t x1 = buffer[idx1], x2 = buffer[idx2];

  buffer[idx2] = x1;
  buffer[idx1] = x2;
  return c;
}

// Randomly select a segment to shuffle in the buffer
Individual ShuffleBytesMutator::apply(utils::Rand &random,
                                      const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);

  if (size < 2) // nothing to shuffle
    return c;

  uint32_t shuffle_amount = random.next_number(std::min(size, (size_t)8)) + 1;
  uint32_t shuffle_start = random.next_number(size - shuffle_amount);
  uint8_t *buffer = c.memory.buffer(c.index);

  random.shuffle(buffer + shuffle_start,
                 buffer + shuffle_start + shuffle_amount);
  return c;
}

// Randomly select a byte in the buffer and duplicate it N times where N is at
// most MAX_INPUT_SIZE_MULTIPLIER
Individual DuplicateByteMutator::apply(utils::Rand &random,
                                       const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);

  uint32_t idx = random.next_number(size);
  uint32_t mul = random.next_number(MAX_INPUT_SIZE_MULTIPLIER) + 1;
  uint8_t *buffer = c.memory.buffer(c.index);
  uint8_t x = buffer[idx];

  c.memory.insert_bytes(c.index, idx, mul);
  buffer = c.memory.buffer(c.index);
  for (uint32_t j = idx; j < idx + mul; j++) {
    buffer[j] = x;
  }
  return c;
}

// Randomly select a range of N bytes in the buffer and duplicate it M times
// where N < MAX_INPUT_SIZE_DUPLICATOR and M is at most
// MAX_INPUT_SIZE_MULTIPLIER
Individual DuplicateBytesMutator::apply(utils::Rand &random,
                                        const Individual &i) const {
  Individual c = i.clone();
  const size_t size = c.memory.length(c.index);

  uint32_t range = random.next_number(MAX_INPUT_SIZE_DUPLICATOR) + 1;
  if (range >= size) {
    return c;
  }

  uint32_t idx = random.next_number(size - range);
  uint32_t mul = random.next_number(MAX_INPUT_SIZE_MULTIPLIER) + 1;

  // LOG(INFO) << "idx=" << idx << " N=" << range << " M=" << mul;

  uint8_t *buffer = c.memory.buffer(c.index);

  c.memory.insert_bytes(c.index, idx + range, mul * range);
  buffer = c.memory.buffer(c.index);

  // LOG(INFO) << " new length=" << c.memory.length(c.index);

  for (size_t insert_pos = idx + range; insert_pos < idx + range + mul * range;
       insert_pos += range) {
    for (size_t N = 0; N < range; N++) {
      buffer[insert_pos + N] = buffer[idx + N];
    }
  }
  return c;
}

// Find blocks of consecutive digits, and mutate the a value that's randomly
// selected.
Individual ChangeASCIIIntegerMutator::apply(utils::Rand &random,
                                            const Individual &i) const {
  Individual c = i.clone();
  size_t size = c.memory.length(c.index);
  uint8_t *buffer = c.memory.buffer(c.index);

  uint32_t finger = 0, start = 0;
  map<uint32_t, uint32_t> integers;
  vector<uint32_t> start_indices;
  while (finger < size) {
    start = finger;
    while (finger < size && std::isdigit(buffer[finger])) {
      finger++;
    }
    if (start < finger) {
      integers.insert({start, finger - start});
      start_indices.push_back(start);
    }
    finger++;
  }

  if (!integers.empty()) {
    uint32_t chosen_start = random.next_number(start_indices.size());
    start = start_indices[chosen_start];
    finger = integers[start] + start - 1; // end

    uint64_t value = buffer[start] - '0';
    uint32_t j = start;
    while (j < finger) {
      value = value * 10 + (buffer[j] - '0');
      j++;
    }

#define NUMBER_DIGIT_MUTATIONS 11
    switch (random.next_number(NUMBER_DIGIT_MUTATIONS)) {
    //
    // Simple number operators (shifts, etc.)
    //
    case 0:
      value++;
      break;
    case 1:
      value--;
      break;
    case 2:
      value <<= 1;
      break;
    case 3:
      value >>= 1;
      break;
    case 4:
      value = random.next_number(value);
      break;
    case 5:
      value = random.next_number(value * value);
      break;
    //
    // Edge values for integers; trying to go after wrap-arounds quickly
    //
    case 6:
      value = 0;
      break;
    case 7:
      value = UINT8_MAX;
      break;
    case 8:
      value = UINT16_MAX;
      break;
    case 9:
      value = UINT32_MAX;
      break;
    case 10:
      value = UINT64_MAX;
      break;
    default:
      break;
    }
#undef NUMBER_DIGIT_MUTATIONS

    // Write the value, grow the output buffer if needed.
    std::string value_str = std::to_string(value);
    finger++; // adjust the end
    if (value_str.size() > (finger - start)) {
      uint32_t new_bytes = value_str.size() - (finger - start);
      c.memory.insert_bytes(c.index, finger, new_bytes);
      assert(size + new_bytes == c.memory.length(c.index));
    } else {
      // XXX prepend '0's
    }
    buffer = c.memory.buffer(c.index);
    for (auto &chr : value_str) {
      buffer[start++] = chr;
    }
  } else {
    // No digit??
  }

  return c;
}

#undef MAX_INPUT_SIZE_MULTIPLIER

// end namespace=ga
}
// end namespace=fuzz
}
