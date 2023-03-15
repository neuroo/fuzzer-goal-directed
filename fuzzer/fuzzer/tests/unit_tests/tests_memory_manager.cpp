
#define BOOST_TEST_MODULE MemoryManager Tests
#include <boost/test/included/unit_test.hpp>

#include "memory-manager.h"
using namespace fuzz;

#include <cstdlib>
#include <cstring>
#include <string>

BOOST_AUTO_TEST_CASE(create_MemoryManager) {
  MemoryManager manager;
  BOOST_TEST(manager.size() == 0);
}

BOOST_AUTO_TEST_CASE(create_MultipleBuffers) {
  MemoryManager manager;

  MemoryManager::triplet_t result = manager.create();
  BOOST_TEST(result.index == 0);
  BOOST_TEST(result.ptr == (uint8_t *)nullptr);
  BOOST_TEST(result.length == 0);
  BOOST_TEST(manager.get_next_index() == 1);

  MemoryManager::triplet_t result2 = manager.create(256);
  BOOST_TEST(result2.index == 1);
  BOOST_TEST(result2.ptr != (uint8_t *)nullptr);
  BOOST_TEST(result2.length == 256);
  BOOST_TEST(manager.get_next_index() == 2);

  MemoryManager::triplet_t result3 = manager.create(512);
  BOOST_TEST(result3.index == 2);
  BOOST_TEST(result3.ptr != (uint8_t *)nullptr);
  BOOST_TEST(result3.length == 512);
  BOOST_TEST(manager.get_next_index() == 3);

  BOOST_TEST(manager.buffer(result3.index) == result3.ptr);
  BOOST_TEST(manager.length(result3.index) == result3.length);

  size_t shallow_copy_index = manager.copy(result3.index, /*shallow*/ true);
  BOOST_TEST(shallow_copy_index == result3.index);

  size_t copy_index = manager.copy(result3.index, /*shallow*/ false);
  BOOST_TEST(copy_index != result3.index);
  BOOST_TEST(manager.length(copy_index) == result3.length);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index),
                         manager.buffer(result3.index),
                         manager.length(copy_index)) == 0);
}

BOOST_AUTO_TEST_CASE(manipulate_Buffers) {
  MemoryManager manager;

  MemoryManager::triplet_t result = manager.create(10);
  std::memcpy(result.ptr, "AAAAAAAAAA", 10);
  size_t copy_index = manager.copy(result.index);

  BOOST_TEST(copy_index != result.index);
  BOOST_TEST(manager.length(copy_index) == result.length);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index),
                         manager.buffer(result.index),
                         manager.length(copy_index)) == 0);

  uint8_t *ptr = manager.buffer(copy_index);
  ptr[1] = 'B';
  ptr[3] = 'B';

  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "ABABAAAAAA",
                         manager.length(copy_index)) == 0);

  manager.remove_bytes(copy_index, 1);
  manager.print_buffer(copy_index);
  BOOST_TEST(manager.length(copy_index) == result.length - 1);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "AABAAAAAA",
                         manager.length(copy_index)) == 0);

  BOOST_TEST(manager.remove_bytes(copy_index, 1, 5) == true);
  manager.print_buffer(copy_index);
  BOOST_TEST(manager.length(copy_index) == result.length - 6);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "AAAA",
                         manager.length(copy_index)) == 0);

  BOOST_TEST(manager.remove_bytes(copy_index, 0, 2) == true);
  manager.print_buffer(copy_index);
  BOOST_TEST(manager.length(copy_index) == result.length - 8);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "AA",
                         manager.length(copy_index)) == 0);

  BOOST_TEST(manager.insert_bytes(copy_index, 0) == true);
  ptr = manager.buffer(copy_index);
  ptr[0] = 'B';
  manager.print_buffer(copy_index);
  BOOST_TEST(manager.length(copy_index) == result.length - 7);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "BAA",
                         manager.length(copy_index)) == 0);

  BOOST_TEST(manager.insert_bytes(copy_index, 1, 2) == true);
  ptr = manager.buffer(copy_index);
  ptr[1] = 'C';
  ptr[2] = 'D';
  manager.print_buffer(copy_index);
  BOOST_TEST(manager.length(copy_index) == result.length - 5);
  BOOST_TEST(std::memcmp(manager.buffer(copy_index), "BCDAA",
                         manager.length(copy_index)) == 0);

  manager.dismiss(result.index);
  manager.dismiss(copy_index);

  BOOST_TEST(manager.size() == 0);
}
