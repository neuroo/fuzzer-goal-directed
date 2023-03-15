#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace fuzz {

#define UNINIT_INDEX 0

// This utility manages all raw buffers used by the individuals. That's mostly
// to centralize all buffer related operators as it doesn't really provide any
// smartness on how the data is managed.
// Ideally, we'd use a rope/trie/flyweight to store raw memory chunks, and
// pass them around as chunks only w/o any copy since there is a lot of
// redundant data. That's only needed if the population gets very large though.
class MemoryManager {
  size_t next_index = (UNINIT_INDEX + 1);
  std::map<size_t, size_t> refs;
  std::map<size_t, uint8_t *> memory;
  std::map<size_t, size_t> lengths;

public:
  struct triplet_t {
    size_t index = UNINIT_INDEX;
    uint8_t *ptr = nullptr;
    size_t length = 0;

    triplet_t() = default;
    triplet_t(const size_t index, uint8_t *ptr, const size_t length)
        : index(index), ptr(ptr), length(length) {}
    triplet_t(const triplet_t &t)
        : index(t.index), ptr(t.ptr), length(t.length) {}
    triplet_t &operator=(const triplet_t &t) {
      if (&t != this) {
        index = t.index;
        ptr = t.ptr;
        length = t.length;
      }
      return *this;
    }
  };

  MemoryManager() = default;
  MemoryManager(const MemoryManager &) = delete;
  MemoryManager &operator=(const MemoryManager &i) { return *this; };
  ~MemoryManager() { cleanup_all(); }

  //
  // Container utils
  //
  size_t size() const { return lengths.size(); }

  size_t get_next_index() const { return next_index; }

  bool is_null(const size_t index) const;

  uint8_t *buffer(const size_t index) const;

  size_t length(const size_t index) const;

  triplet_t create(const size_t new_size = 0);

  void dismiss(const size_t index);

  size_t copy(const size_t source_index, const bool shallow = false);

  // Remove some bytes from the current buffer
  bool remove_bytes(const size_t index, const size_t start_index = 0,
                    const size_t number_bytes = 1);

  bool insert_bytes(const size_t index, const size_t start_index = 0,
                    const size_t number_bytes = 1);

  void print_buffer(const size_t index);

  void increase_count(const size_t index);

  void decrease_count(const size_t index);

  void debug_print() const;

  void force_clean(const std::set<size_t> &indices);

private:
  // Cannot do that with reference counting.
  bool resize_buffer(const size_t index, const size_t new_size);

  void cleanup_all();
};
}

#endif
