/*
 * File:   lrucache.hpp
 * Author: Alexander Ponomarev
 *         Romain Gaucher
 *
 * Created on June 20, 2013, 5:09 PM
 * Modified on Jan 12, 2016
 */

#ifndef _LRUCACHE_HPP_INCLUDED_
#define _LRUCACHE_HPP_INCLUDED_

#include <cstddef>
#include <list>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace cache {

// XXX use defaults for hash_key_t and equal_key_t
template <class key_t, class value_t, class hash_key_t, class equal_to_key_t>
class lru_cache {
public:
  typedef typename std::pair<key_t, value_t> key_value_pair_t;
  typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

  lru_cache(size_t max_size) : _max_size(max_size) {}

  void put(const key_t &key, const value_t &value) {
    std::lock_guard<std::mutex> lock(map_guard);
    auto it = _cache_items_map.find(key);
    if (it != _cache_items_map.end()) {
      _cache_items_list.erase(it->second);
      _cache_items_map.erase(it);
    }

    _cache_items_list.push_front(key_value_pair_t(key, value));
    _cache_items_map[key] = _cache_items_list.begin();

    if (_cache_items_map.size() > _max_size) {
      auto last = _cache_items_list.end();
      last--;
      _cache_items_map.erase(last->first);
      _cache_items_list.pop_back();
    }
  }

  const value_t &get(const key_t &key) {
    std::lock_guard<std::mutex> lock(map_guard);
    auto it = _cache_items_map.find(key);
    if (it == _cache_items_map.end()) {
      throw std::range_error("There is no such key in cache");
    } else {
      _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list,
                               it->second);
      return it->second->second;
    }
  }

  const value_t copy_get(const key_t &key) {
    std::lock_guard<std::mutex> lock(map_guard);
    auto it = _cache_items_map.find(key);
    if (it == _cache_items_map.end()) {
      throw std::range_error("There is no such key in cache");
    } else {
      _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list,
                               it->second);
      return it->second->second;
    }
  }

  bool exists(const key_t &key) const {
    return _cache_items_map.find(key) != _cache_items_map.end();
  }

  size_t size() const { return _cache_items_map.size(); }

private:
  std::list<key_value_pair_t> _cache_items_list;
  std::unordered_map<key_t, list_iterator_t, hash_key_t, equal_to_key_t>
      _cache_items_map;
  size_t _max_size;
  std::mutex map_guard;
};

} // namespace lru

#endif /* _LRUCACHE_HPP_INCLUDED_ */
