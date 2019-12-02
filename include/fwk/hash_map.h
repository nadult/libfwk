// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/hash.h"
#include "fwk/sys/memory.h"

namespace fwk {

// Original source: hash_map.h from RDE STL by Maciej Sinilo (Copyright 2007)
// Licensed under MIT license
// Improved & adapted for libfwk by Krzysztof Jakubowski
template <typename TKey, typename TValue> class HashMap {
  public:
	using Key = TKey;
	using Value = TValue;
	using KeyValuePair = Pair<TKey, TValue>;
	using HashValue = u32;

	static constexpr HashValue unused_hash = 0xffffffff;
	static constexpr HashValue deleted_hash = 0xfffffffe;
	// Index is occupied if hash < deleted_hash

	static constexpr int initial_capacity = 64;
	static_assert(isPowerOfTwo(initial_capacity));

	template <bool is_const> struct TIter {
		template <bool to_const, EnableIf<!is_const && to_const>...> operator TIter<to_const>() {
			return {map, idx};
		}

		auto &operator*() const { return PASSERT(!atEnd()), map->m_key_values[idx]; }
		auto *operator-> () const { return PASSERT(!atEnd()), &map->m_key_values[idx]; }
		void operator++() { PASSERT(!atEnd()), ++idx, moveToNextOccupiedNode(); }
		bool operator==(const TIter &rhs) const { return rhs.idx == idx; }

		bool atEnd() const { return idx >= map->m_capacity; }
		explicit operator bool() const { return idx < map->m_capacity; }

		void moveToNextOccupiedNode() {
			while(idx < map->m_capacity && map->m_hashes[idx] >= deleted_hash)
				idx++;
		}

		If<is_const, const HashMap *, HashMap *> map;
		int idx;
	};

	using Iter = TIter<false>;
	using ConstIter = TIter<true>;

	HashMap() {}
	explicit HashMap(int initial_bucket_count) { reserve(initial_bucket_count); }
	HashMap(const HashMap &rhs) { *this = rhs; }
	~HashMap() { deleteNodes(); }

	// Load factor controls hash map load. Default is ~66%.
	// Higher factor means tighter maps and bigger risk of collisions.
	void setLoadFactor(float factor) {
		PASSERT(factor >= 0.125f && factor <= 1.0f);
		m_load_factor = factor;
		m_used_limit = (int)(m_capacity * factor);
	}
	float loadFactor() const { return m_load_factor; }

	Iter begin() {
		Iter it{this, 0};
		it.moveToNextOccupiedNode();
		return it;
	}
	ConstIter begin() const {
		ConstIter it{this, 0};
		it.moveToNextOccupiedNode();
		return it;
	}

	Iter end() { return {this, m_capacity}; }
	ConstIter end() const { return {this, m_capacity}; }

	Value &operator[](const Key &key) {
		HashValue hash = hashFunc(key);
		int idx = findForInsert(key, hash);
		if(idx == m_capacity || m_hashes[idx] >= deleted_hash)
			return emplaceAt({key, TValue()}, idx, hash).first->second;
		return m_key_values[idx].second;
	}

	void operator=(const HashMap &rhs) {
		if(&rhs == this)
			return;

		clear();
		if(m_capacity < rhs.m_capacity) {
			deleteNodes();
			m_hashes = allocateHashes(rhs.m_capacity);
			m_key_values = (KeyValuePair *)fwk::allocate(rhs.m_capacity * sizeof(KeyValuePair));
			m_capacity = rhs.m_capacity;
			m_capacity_mask = m_capacity - 1;
		}
		rehash(m_capacity, m_hashes, m_key_values, rhs.m_capacity, rhs.m_hashes, rhs.m_key_values,
			   false);
		m_size = rhs.size();
		m_num_used = rhs.m_num_used;
		setLoadFactor(rhs.m_load_factor);
	}

	void swap(HashMap &rhs) {
		if(&rhs != this) {
			swap(m_hashes, rhs.m_hashes);
			swap(m_key_values, rhs.m_key_values);
			swap(m_size, rhs.m_size);
			swap(m_capacity, rhs.m_capacity);
			swap(m_capacity_mask, rhs.m_capacity_mask);
			swap(m_used_limit, rhs.m_used_limit);
			swap(m_num_used, rhs.m_num_used);
			swap(m_load_factor, rhs.m_load_factor);
		}
	}

	Pair<Iter, bool> emplace(const TKey &key, const TValue &value) { return emplace({key, value}); }

	Pair<Iter, bool> emplace(const KeyValuePair &v) {
		if(m_num_used >= m_used_limit)
			grow();

		HashValue hash = hashFunc(v.first);
		int idx = findForInsert(v.first, hash);
		if(m_hashes[idx] < deleted_hash)
			return {{this, idx}, false};
		if(m_hashes[idx] == unused_hash)
			++m_num_used;
		new(&m_key_values[idx]) KeyValuePair(v);
		m_hashes[idx] = hash;
		++m_size;
		PASSERT(m_num_used >= m_size);
		return {{this, idx}, true};
	}

	bool erase(const Key &key) {
		auto idx = lookup(key);
		if(idx != m_capacity && m_hashes[idx] < deleted_hash) {
			eraseNode(idx);
			return true;
		}
		return false;
	}

	void erase(Iter it) {
		PASSERT(valid(it));
		if(it != end())
			eraseNode(it.idx);
	}

	void erase(Iter from, Iter to) {
		PASSERT(valid(from) && valid(to));

		auto idx = from.idx;
		while(idx != to.idx) {
			if(m_hashes[idx] < deleted_hash)
				eraseNode(idx);
			++idx;
		}
	}

	Iter find(const Key &key) { return {this, lookup(key)}; }
	ConstIter find(const Key &key) const { return {this, lookup(key)}; }
	Maybe<Value> maybeFind(const Key &key) const {
		auto it = find(key);
		if(it == end())
			return none;
		return it->second;
	}

	void clear() {
		for(int n = 0; n < m_capacity; n++) {
			if(m_hashes[n] < deleted_hash)
				m_key_values[n].~KeyValuePair();
			m_hashes[n] = unused_hash;
		}
		m_size = m_num_used = 0;
	}

	// TODO: something is wrong here (when reserving space in voxelizer/tri_points procedure
	// slows down drastically (slower the bigger reserve))
	void reserve(int min_size) {
		int new_capacity = m_capacity == 0 ? initial_capacity : m_capacity;
		while(new_capacity < min_size)
			new_capacity *= 2;
		if(new_capacity > m_capacity)
			grow(new_capacity);
	}

	int capacity() const { return m_capacity; }
	int size() const { return m_size; }
	bool empty() const { return size() == 0; }

	int usedBucketCount() const { return m_num_used; }
	int usedMemory() const { return capacity() * (sizeof(HashValue) + sizeof(KeyValuePair)); }

	CSpan<HashValue> hashes() const { return {m_hashes, m_capacity}; }
	KeyValuePair *data() { return m_key_values; }
	const KeyValuePair *data() const { return m_key_values; }

	vector<Value> values() const {
		vector<Value> out;
		out.reserve(size());
		for(auto &pair : *this)
			out.emplace_back(pair.second);
		return out;
	}

	vector<Key> keys() const {
		vector<Key> out;
		out.reserve(size());
		for(auto &pair : *this)
			out.emplace_back(pair.first);
		return out;
	}

	vector<Pair<Key, Value>> pairs() const { return transform<Pair<Key, Value>>(*this); }

	template <bool is_const> bool valid(TIter<is_const> iter) const {
		return iter.idx >= 0 && iter.idx <= m_capacity && iter.map == this;
	}

  private:
	void grow() { grow(m_capacity == 0 ? initial_capacity : m_capacity * 2); }
	void grow(int new_capacity) {
		PASSERT(isPowerOfTwo(new_capacity));
		auto *new_hashes = allocateHashes(new_capacity);
		auto *new_key_values = (KeyValuePair *)fwk::allocate(new_capacity * sizeof(KeyValuePair));

		rehash(new_capacity, new_hashes, new_key_values, m_capacity, m_hashes, m_key_values, true);
		if(m_hashes != &s_empty_node)
			fwk::deallocate(m_hashes);
		fwk::deallocate(m_key_values);

		m_capacity = new_capacity;
		m_capacity_mask = new_capacity - 1;
		m_used_limit = (int)(m_capacity * m_load_factor);
		m_hashes = new_hashes;
		m_key_values = new_key_values;
		m_num_used = m_size;
		PASSERT(m_num_used < m_capacity);
	}

	Pair<Iter, bool> emplaceAt(const KeyValuePair &v, int idx, HashValue hash) {
		if(idx == m_capacity || m_num_used >= m_used_limit)
			return emplace(v);

		PASSERT(m_hashes[idx] >= deleted_hash);
		if(m_hashes[idx] == unused_hash)
			++m_num_used;
		new(&m_key_values[idx]) KeyValuePair(v);
		m_hashes[idx] = hash;
		++m_size;
		return {{this, idx}, true};
	}

	int findForInsert(const Key &key, HashValue hash) {
		// TODO: unsigned or signed ?
		int idx = hash & m_capacity_mask;
		if(m_hashes[idx] == hash && m_key_values[idx].first == key)
			return idx;
		if(idx == m_capacity)
			return m_capacity;

		int free_idx = m_hashes[idx] == deleted_hash ? idx : -1;

		// Guarantees loop termination.
		PASSERT(m_num_used < m_capacity);

		unsigned num_probes = 0;
		while(m_hashes[idx] <= deleted_hash) {
			num_probes++;
			idx = (idx + num_probes) & m_capacity_mask;
			if(m_hashes[idx] == hash && m_key_values[idx].first == key)
				return idx;
			if(m_hashes[idx] == deleted_hash && free_idx == -1)
				free_idx = idx;
		}
		return free_idx != -1 ? free_idx : idx;
	}

	int lookup(const Key &key) const {
		auto hash = hashFunc(key);
		unsigned idx = hash & m_capacity_mask;
		if(m_hashes[idx] == hash && m_key_values[idx].first == key)
			return idx;

		// Guarantees loop termination.
		PASSERT(m_capacity == 0 || m_num_used < m_capacity);

		unsigned num_probes = 0;
		while(m_hashes[idx] <= deleted_hash) {
			num_probes++;
			idx = (idx + num_probes) & m_capacity_mask;
			if(m_hashes[idx] == hash && m_key_values[idx].first == key)
				return idx;
		}
		return m_capacity;
	}

	static void rehash(int new_capacity, HashValue *new_hashes, KeyValuePair *new_key_values,
					   int capacity, const HashValue *hashes, const KeyValuePair *key_values,
					   bool destruct_original) {
		const u32 mask = new_capacity - 1;
		for(int idx = 0; idx < capacity; idx++) {
			if(hashes[idx] < deleted_hash) {
				const HashValue hash = hashes[idx];
				u32 i = hash & mask;

				unsigned num_probes = 0;
				while(new_hashes[i] != unused_hash) {
					++num_probes;
					i = (i + num_probes) & mask;
				}
				new(&new_key_values[i]) KeyValuePair(key_values[idx]);
				new_hashes[i] = hash;
				if(destruct_original)
					key_values[idx].~KeyValuePair();
			}
		}
	}

	HashValue *allocateHashes(int count) {
		auto *hashes = static_cast<HashValue *>(fwk::allocate(count * sizeof(HashValue)));
		for(int n = 0; n < count; n++)
			hashes[n] = unused_hash;
		return hashes;
	}

	void deleteNodes() {
		for(int n = 0; n < m_capacity; n++)
			if(m_hashes[n] < deleted_hash)
				m_key_values[n].~KeyValuePair();
		if(m_hashes != &s_empty_node)
			fwk::deallocate(m_hashes);
		fwk::deallocate(m_key_values);
		m_capacity = m_size = m_capacity_mask = m_used_limit = 0;
	}

	void eraseNode(int idx) {
		PASSERT(m_hashes[idx] < deleted_hash);
		m_key_values[idx].~KeyValuePair();
		m_hashes[idx] = deleted_hash;
		--m_size;
	}

	HashValue hashFunc(const Key &key) const { return hash<HashValue>(key) & 0x7FFFFFFFu; }

	static inline HashValue s_empty_node = unused_hash;
	HashValue *m_hashes = &s_empty_node;
	KeyValuePair *m_key_values = nullptr;
	int m_size = 0, m_capacity = 0;
	int m_num_used = 0, m_used_limit = 0;
	float m_load_factor = 2.0f / 3.0f;
	u32 m_capacity_mask = 0;
	template <bool is_const> friend struct TIter;
};
}
