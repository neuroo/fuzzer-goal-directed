#include "memory-manager.h"
#include "common/logger.h"
#include "utils.h"

#include <iomanip>
#include <iostream>
#include <sstream>
using namespace std;

namespace fuzz {

void MemoryManager::debug_print() const {
  LOG(INFO) << "MemoryManager::dump";
  for (auto mem_iter = memory.begin(); mem_iter != memory.end(); ++mem_iter) {
    const size_t index = mem_iter->first;
    const size_t references = refs.find(index)->second;
    const size_t length = lengths.find(index)->second;

    ostringstream oss;
    oss << "Slot #" << index << " refs=" << references << " length=" << length;
    LOG(INFO) << oss.str();
  }
}

bool MemoryManager::is_null(const size_t index) const {
  return buffer(index) == nullptr;
}

uint8_t *MemoryManager::buffer(const size_t index) const {
  if (index == UNINIT_INDEX || index >= next_index)
    return nullptr;

  map<size_t, uint8_t *>::const_iterator buf_iter = memory.find(index);
  if (buf_iter == memory.end())
    return nullptr;
  return buf_iter->second;
}

size_t MemoryManager::length(const size_t index) const {
  if (index == UNINIT_INDEX || index >= next_index)
    return 0;

  map<size_t, size_t>::const_iterator length_iter = lengths.find(index);
  if (length_iter == lengths.end())
    return 0;
  return length_iter->second;
}

MemoryManager::triplet_t MemoryManager::create(const size_t new_size) {
  size_t current_index = next_index;
  ++next_index;
  size_t adapted_size = new_size < 1 ? 1 : new_size;

  // Actually allocate something and store the size
  uint8_t *new_buffer = new (std::nothrow) uint8_t[adapted_size];

  refs.insert({current_index, 1});
  lengths.insert({current_index, new_size});
  memory.insert({current_index, new_buffer});

  return triplet_t(current_index, new_buffer, new_size);
}

void MemoryManager::dismiss(const size_t index) {
  if (index == UNINIT_INDEX)
    return;
  map<size_t, uint8_t *>::iterator mem_ref = memory.find(index);
  if (mem_ref == memory.end())
    return;

  if (mem_ref->second != nullptr) {
    delete[] mem_ref->second;
    mem_ref->second = nullptr;
  }
  memory.erase(mem_ref);

  map<size_t, size_t>::iterator length_ref = lengths.find(index);
  if (length_ref != lengths.end()) {
    lengths.erase(length_ref);
  }

  map<size_t, size_t>::iterator ref_ref = refs.find(index);
  if (ref_ref != refs.end()) {
    refs.erase(ref_ref);
  }
}

size_t MemoryManager::copy(const size_t source_index, const bool shallow) {
  if (source_index == UNINIT_INDEX || source_index >= next_index)
    return UNINIT_INDEX;

  if (shallow) {
    increase_count(source_index);
    return source_index;
  }
  triplet_t n = create(length(source_index));
  if (n.length > 0) {
    std::memcpy(n.ptr, memory[source_index], n.length);
  }
  return n.index;
}

// Remove some bytes from the current buffer
bool MemoryManager::remove_bytes(const size_t index, const size_t start_index,
                                 const size_t number_bytes) {
  if (index == UNINIT_INDEX || index >= next_index)
    return false;

  size_t buffer_length = length(index);
  uint8_t *ptr = buffer(index);
  size_t remaining_start = start_index + number_bytes;

  std::memmove(ptr + start_index, ptr + remaining_start,
               buffer_length - number_bytes - start_index);
  lengths[index] = buffer_length - number_bytes;
  ptr[buffer_length - number_bytes] = '\0';
  return true;
}

bool MemoryManager::insert_bytes(const size_t index, const size_t start_index,
                                 const size_t number_bytes) {
  if (index == UNINIT_INDEX || index >= next_index)
    return false;

  size_t buffer_length = length(index);
  if (start_index < buffer_length &&
      resize_buffer(index, buffer_length + number_bytes)) {
    size_t after_new_bytes = start_index + number_bytes;
    uint8_t *ptr = buffer(index);
    std::memmove(ptr + after_new_bytes, ptr + start_index,
                 buffer_length - start_index);
    return true;
  } else {
    if (resize_buffer(index, buffer_length + number_bytes)) {
      return true;
    }
  }
  return false;
}

void MemoryManager::print_buffer(const size_t index) {
  if (index == UNINIT_INDEX || index >= next_index)
    return;

  std::cout << "length=" << length(index)
            << " buffer=" << std::setw(length(index)) << buffer(index)
            << std::endl;
}

void MemoryManager::increase_count(const size_t index) {
  if (index == UNINIT_INDEX || index >= next_index)
    return;
  map<size_t, size_t>::iterator ref_iter = refs.find(index);
  if (ref_iter == refs.end())
    return;
  ref_iter->second = ref_iter->second + 1;
}

void MemoryManager::decrease_count(const size_t index) {
  if (index == UNINIT_INDEX || index >= next_index)
    return;
  map<size_t, size_t>::iterator ref_iter = refs.find(index);
  if (ref_iter == refs.end())
    return;
  if (ref_iter->second - 1 < 1) {
    dismiss(index);
  } else {
    ref_iter->second = ref_iter->second - 1;
  }
}

// Cannot do that with reference counting.
bool MemoryManager::resize_buffer(const size_t index, const size_t new_size) {
  if (index == UNINIT_INDEX || index >= next_index)
    return false;

  map<size_t, uint8_t *>::iterator buf_iter = memory.find(index);
  if (buf_iter == memory.end())
    return false;

  uint8_t *old_ptr = buf_iter->second;
  if (new_size > 0) {
    uint8_t *new_buffer = new (std::nothrow) uint8_t[new_size];
    if (old_ptr != nullptr && new_buffer != nullptr) {
      std::memcpy(new_buffer, old_ptr, length(index));
      delete[] old_ptr;
    }
    buf_iter->second = new_buffer;
  } else {
    if (old_ptr != nullptr) {
      delete[] old_ptr;
      buf_iter->second = nullptr;
    }
  }
  lengths[index] = new_size;
  return true;
}

void MemoryManager::force_clean(const set<size_t> &active_indices) {
  set<size_t> inactive_indices;
  for (auto ref_iter : refs) {
    if (active_indices.find(ref_iter.first) != active_indices.end())
      continue;
    inactive_indices.insert(ref_iter.first);
  }

  for (auto &ind : inactive_indices) {
    dismiss(ind);
  }
}

void MemoryManager::cleanup_all() {
  LOG(INFO) << "Cleanup all memory...";
  if (memory.empty())
    return;
  for (map<size_t, uint8_t *>::iterator mem_iter = memory.begin();
       mem_iter != memory.end(); ++mem_iter) {
    if (mem_iter->second != nullptr) {
      delete[] mem_iter->second;
      mem_iter->second = nullptr;
    }
  }
}
}
