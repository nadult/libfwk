// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vector.h"

namespace fwk {

// Map based on sorted vector (flat structure, linear insertion time)
//
// TODO: Does it really make sense to use it? Why not use unordered_map?
// When searching for values, it seems to be faster
// TODO: rename to OrderedVector or something ?
template <class Key, class Value> class VectorMap {
  public:
	using Pair = pair<Key, Value>;
	using const_iterator = typename vector<Pair>::const_iterator;
	using iterator = typename vector<Pair>::iterator;

	// TODO: better name (strongly ordered or smth)
	static bool isSortedAndUnique(CSpan<Pair> range) {
		for(int n = 1; n < range.size(); n++)
			if(!(range[n - 1].first < range[n].first))
				return false;
		return true;
	}

	VectorMap() = default;
	VectorMap(vector<Pair> elements) : m_container(move(elements)) {
		DASSERT(isSortedAndUnique(m_container));
	}

	struct Compare {
		bool operator()(const Pair &lhs, const Pair &rhs) const { return lhs.first < rhs.first; }
		bool operator()(const Pair &lhs, const Key &key) const { return lhs.first < key; }
		bool operator()(const Key &key, const Pair &rhs) const { return key < rhs.first; }
	};

	int size() const { return (int)m_container.size(); }
	auto begin() const { return m_container.begin(); }
	auto end() const { return m_container.end(); }
	auto begin() { return m_container.begin(); }
	auto end() { return m_container.end(); }

	explicit operator bool() const { return !empty(); }
	bool empty() const { return m_container.empty(); }

	auto lower_bound(const Key &key) const {
		return std::lower_bound(begin(), end(), key, Compare());
	}
	auto upper_bound(const Key &key) const {
		return std::upper_bound(begin(), end(), key, Compare());
	}
	auto lower_bound(const Key &key) { return std::lower_bound(begin(), end(), key, Compare()); }
	auto upper_bound(const Key &key) { return std::upper_bound(begin(), end(), key, Compare()); }

	auto find(const Key &key) {
		auto it = lower_bound(key);
		return it == end() || !Compare()(key, *it) ? it : end();
	}
	auto find(const Key &key) const {
		auto it = lower_bound(key);
		return it == end() || !Compare()(key, *it) ? it : end();
	}

	Maybe<Value> maybeFind(const Key &key) const {
		auto it = find(key);
		return it == end() ? none : Maybe<Value>(it->second);
	}
	Value find(const Key &key, Value when_not_found) const {
		auto it = find(key);
		return it == end() ? when_not_found : it->second;
	}

	pair<iterator, bool> insert(Pair &&pair) {
		auto it = lower_bound(pair.first);
		if(it == end() || Compare()(pair.first, *it))
			return {m_container.insert(it, move(pair)), true};
		return {it, false};
	}

	void erase(const Key &key) {
		auto it = lower_bound(key);
		if(it != end())
			m_container.erase(it);
	}

	void erase(const_iterator it) { m_container.erase(it); }
	void clear() { m_container.clear(); }

	void reserve(int size) { m_container.reserve(size); }

	template <class Predicate> void erase_if(Predicate pred) {
		auto new_end = std::remove_if(m_container.begin(), m_container.end(), pred);
		m_container.erase(new_end, m_container.end());
	}

	Value &operator[](const Key &key) {
		auto it = lower_bound(key);
		if(it == end() || Compare()(key, *it))
			it = m_container.insert(it, {key, Value()});
		return m_container[it - begin()].second;
	}

	Pair &atIndex(int index) { return m_container[index]; }
	const Pair &atIndex(int index) const { return m_container[index]; }

  private:
	vector<Pair> m_container;
};

template <class T1, class T2> pair<T2, T1> invertPair(const pair<T1, T2> &p) {
	return {p.second, p.first};
}

}
