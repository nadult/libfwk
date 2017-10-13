// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_CACHE_H
#define FWK_CACHE_H

#include "fwk/sys/immutable_ptr.h"
#include "fwk_base.h"
#include <map>
#include <mutex>

namespace fwk {

// TODO: nullptr should be treated differently in some cases
//      (when get_immutable_ptr() returns null, cache should be disabled)
// TODO: option to limit cache size
// TODO: more control over caches? so that they could be used as drawing
// caches, not only computation caches

namespace cache {

	// TODO: option to verify object state (with method isValid or something)

	template <class T> static bool isValidKeyElement(const T &elem) { return true; }
	template <class T> static bool isValidKeyElement(const immutable_weak_ptr<T> &elem) {
		return !elem.expired();
	}

	template <std::size_t I = 0, typename... Tp>
	typename std::enable_if<I == sizeof...(Tp), bool>::type isValidKey(const std::tuple<Tp...> &) {
		return true;
	}

	template <std::size_t I = 0, typename... Tp>
		typename std::enable_if <
		I<sizeof...(Tp), bool>::type isValidKey(const std::tuple<Tp...> &t) {
		return isValidKeyElement(std::get<I>(t)) && isValidKey<I + 1, Tp...>(t);
	}
}

template <class Value, class Key> class CacheImpl {
  public:
	CacheImpl() : m_next_clear_cycle(4) {}

	void add(const Key &key, immutable_ptr<Value> value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_map.emplace(key, move(value));
		if(!--m_next_clear_cycle) {
			clearInvalid();
			m_next_clear_cycle = m_map.size();
		}
	}

	auto access(const Key &key) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_map.find(key);
		if(it != m_map.end()) {
			return it->second;
		}
		return immutable_ptr<Value>();
	}

	void remove(const Key &key) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_map.erase(m_map.find(key));
		m_next_clear_cycle--;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_map.clear();
		m_next_clear_cycle = 4;
	}

	static CacheImpl &instance() {
		static CacheImpl instance;
		return instance;
	}

  private:
	template <class T> static bool isValidElem(const T &elem) { return true; }
	template <class T> static bool isValidElem(const immutable_weak_ptr<T> &elem) {
		return !elem.expired();
	}

	template <std::size_t I = 0, typename... Tp>
	typename std::enable_if<I == sizeof...(Tp), bool>::type isValid(const std::tuple<Tp...> &) {
		return true;
	}

	template <std::size_t I = 0, typename... Tp>
		typename std::enable_if < I<sizeof...(Tp), bool>::type isValid(const std::tuple<Tp...> &t) {
		return isValidElem(std::get<I>(t)) && isValid<I + 1, Tp...>(t);
	}

	void clearInvalid() {
		for(auto it = begin(m_map); it != end(m_map);) {
			auto current = it++;
			if(!cache::isValidKey(current->first))
				m_map.erase(current);
		}
	}

	// TODO: shared_mutex
	mutable std::mutex m_mutex;
	std::map<Key, immutable_ptr<Value>> m_map;
	size_t m_next_clear_cycle;
};

class Cache {
  public:
	// TODO: memory limit for each cache?
	static void clearInvalid();

	template <class T> struct KeyFilter { using type = T; };
	template <class T> struct KeyFilter<immutable_ptr<T>> { using type = immutable_weak_ptr<T>; };

	// At least one key element should be expirable, otherwise it will never
	// be removed from the cache; unless explicitly removed by the user
	template <class... Keys> static auto makeKey(Keys... keys) {
		return std::make_tuple((typename KeyFilter<Keys>::type)(keys)...);
	}

	template <class Value, class Key> static void add(const Key &key, immutable_ptr<Value> value) {
		auto &instance = CacheImpl<Value, Key>::instance();
		instance.add(key, move(value));
	}

	template <class Value, class Key> static auto access(const Key &key) {
		auto &instance = CacheImpl<Value, Key>::instance();
		return instance.access(key);
	}

	template <class Value, class Key> static void clear() {
		auto &instance = CacheImpl<Value, Key>::instance();
		instance.clear();
	}

	template <class Value, class Key> static void remove(const Key &key) {
		auto &instance = CacheImpl<Value, Key>::instance();
		instance.remove(key);
	}
};
}

#endif
