// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/hash_map_storage.h"
#include "fwk/math/hash.h"
#include "fwk/vector.h"

namespace fwk {

// fwk::HashMap evolved from rdestl::hash_map by Maciej Sinilo (Copyright 2007)
// Licensed under MIT license

enum class HashMapStorage { paired, separated, paired_with_hashes, automatic };

namespace detail {

	template <class K, class V>
	using DefaultHashMapStorage = If<intrusive_hash_type<K>, HashMapStoragePaired<K, V>,
									 HashMapStoragePairedWithHashes<K, V>>;

	template <class K, class V, class P> struct AccessHashMapPolicy {
		FWK_SFINAE_TEST(has_storage, P, U::storage());
		static constexpr auto storage = []() {
			if constexpr(has_storage)
				return P::storage();
			return HashMapStorage::automatic;
		}();

		FWK_SFINAE_TYPE(Storage_, P, DECLVAL(typename U::Storage));
		using Storage = If<is_same<Storage_, InvalidType>, DefaultHashMapStorage<K, V>, Storage_>;

		FWK_SFINAE_TYPE(HashResult, P, U::hash(DECLVAL(const K &)));
		static constexpr bool has_hash = is_convertible<HashResult, u32>;

		FWK_SFINAE_TYPE(DefaultValueResult, P, U::defaultValue());
		static constexpr bool has_default_value = is_same<DefaultValueResult, V>;
	};
}

// HashMap with open addressing and a variant of quadratic probing (visits all keys).
// Different storage types are available (see hash_map_storage.h).
//
// User can specify his own policy class, which can contain:
// - Storage type (one of HashMapStorage*<K,V>)
// - hash function (Policy::hash(const Key&)
// - default value function (Policy::defaultValue())
//
// TODO: sharing code betwee HashMap & HashSet
// TODO: ability to declare HashMap<K, V> without defining K & V
// TODO: use robin hood hash? With time hash table will accumulate deleted hashes and grow
//       or maybe some other method to clear deleted nodes with time?
// TODO: use HashMap<> together with SparseVector<> for big objects?
// TODO: two rehashes: one with move, one with copy
template <class K, class V, class Policy> class HashMap {
  public:
	using Hash = u32;
	using Key = K;
	using Value = V;
	using KeyValue = fwk::KeyValue<const Key, Value>;
	using PolicyInfo = detail::AccessHashMapPolicy<K, V, Policy>;

	using ST = HashMapStorage;
	static constexpr auto storage =
		PolicyInfo::storage == ST::automatic
			? intrusive_hash_type<K> ? ST::paired : ST::paired_with_hashes
			: PolicyInfo::storage;
	static constexpr bool keeps_hashes = storage == ST::paired_with_hashes;
	static constexpr bool keeps_pairs = storage != ST::separated;

	using Storage = If<storage == ST::paired, HashMapStoragePaired<K, V>,
					   If<storage == ST::separated, HashMapStorageSeparated<K, V>,
						  HashMapStoragePairedWithHashes<K, V>>>;

	template <bool is_const> class TIter {
	  public:
		using MapPtr = If<is_const, const HashMap *, HashMap *>;
		using KVRef = If<keeps_pairs, KeyValue &, KeyValue>;

		template <bool to_const, EnableIf<!is_const && to_const>...> operator TIter<to_const>() {
			return {map, idx};
		}

		KVRef operator*() const { return PASSERT(!atEnd()), map->m_storage.keyValue(idx); }
		auto operator-> () const {
			PASSERT(!atEnd());
			if constexpr(keeps_pairs)
				return &map->m_storage.keyValue(idx);
			else {
				struct Proxy {
					fwk::KeyValue<const Key &, If<is_const, const Value, Value> &> ref;
					auto *operator-> () const ALWAYS_INLINE { return &ref; }
				};
				return Proxy{{map->m_storage.key(idx), map->m_storage.value(idx)}};
			}
		}

		auto &key() const { return PASSERT(!atEnd()), map->m_storage.key(idx); }
		auto &value() const { return PASSERT(!atEnd()), map->m_storage.value(idx); }
		void operator++() { PASSERT(!atEnd()), ++idx, skipUnoccupied(); }
		bool operator==(const TIter &rhs) const { return rhs.idx == idx; }

		bool atEnd() const { return idx >= map->m_capacity; }
		explicit operator bool() const { return idx < map->m_capacity; }

		void skipUnoccupied() {
			while(idx < map->m_capacity && !map->m_storage.isValid(idx))
				idx++;
		}

		MapPtr map;
		int idx;
	};

	using Iter = TIter<false>;
	using ConstIter = TIter<true>;

	HashMap() {}
	explicit HashMap(int min_reserve) { reserve(min_reserve); }
	HashMap(const HashMap &rhs) { *this = rhs; }
	HashMap(HashMap &&rhs) { *this = move(rhs); }
	~HashMap() { deleteNodes(); }

	// Load factor controls hash map load. Default is ~66%.
	// Higher factor means tighter maps and bigger risk of collisions.
	void setLoadFactor(float factor) {
		PASSERT(factor >= 0.125f && factor <= 0.9f);
		m_load_factor = factor;
		m_used_limit = (int)(m_capacity * factor);
	}
	float loadFactor() const { return m_load_factor; }

	Iter begin() {
		Iter it{this, 0};
		it.skipUnoccupied();
		return it;
	}
	ConstIter begin() const {
		ConstIter it{this, 0};
		it.skipUnoccupied();
		return it;
	}

	Iter end() { return {this, m_capacity}; }
	ConstIter end() const { return {this, m_capacity}; }

	Value &operator[](const Key &key) {
		auto hash = hashFunc(key);
		int idx = findForInsert(key, hash);
		if(idx == m_capacity || !m_storage.isValid(idx))
			return emplaceNew(idx, key, hash).first.value();
		return m_storage.value(idx);
	}

	void operator=(const HashMap &rhs) {
		if(&rhs == this)
			return;

		clear();
		if(m_capacity < rhs.m_capacity) {
			deleteNodes();
			m_storage = Storage::allocate(rhs.m_capacity);
			m_capacity = rhs.m_capacity;
			m_capacity_mask = m_capacity - 1;
		}
		rehash(m_capacity, m_storage, rhs.m_capacity, (Storage &)rhs.m_storage, false);
		m_size = rhs.m_size;
		m_num_used = rhs.m_num_used;
		setLoadFactor(rhs.m_load_factor);
	}

	void operator=(HashMap &&rhs) {
		deleteNodes();
		memcpy(this, &rhs, sizeof(HashMap));
		memset(&rhs, 0, sizeof(HashMap));
		rhs.m_load_factor = m_load_factor;
	}

	void swap(HashMap &rhs) {
		if(&rhs == this)
			return;
		swap(m_storage, rhs.m_storage);
		swap(m_size, rhs.m_size);
		swap(m_capacity, rhs.m_capacity);
		swap(m_capacity_mask, rhs.m_capacity_mask);
		swap(m_used_limit, rhs.m_used_limit);
		swap(m_num_used, rhs.m_num_used);
		swap(m_load_factor, rhs.m_load_factor);
	}

	auto emplace(const KeyValue &pair) { return emplace(pair.key, pair.value); }
	auto emplace(const Pair<Key, Value> &pair) { return emplace(pair.first, pair.second); }

	template <class... Args> Pair<Iter, bool> emplace(const Key &key, Args &&... args) {
		if(m_num_used >= m_used_limit)
			grow();

		auto hash = hashFunc(key);
		int idx = findForInsert(key, hash);
		if(m_storage.isValid(idx))
			return {{this, idx}, false};
		if(m_storage.isUnused(idx))
			++m_num_used;
		m_storage.construct(idx, hash, key, std::forward<Args>(args)...);
		++m_size;
		PASSERT(m_num_used >= m_size);
		return {{this, idx}, true};
	}

	bool erase(const Key &key) {
		auto idx = lookup(key);
		if(idx != m_capacity && m_storage.isValid(idx)) {
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
			if(m_storage.isValid(idx))
				eraseNode(idx);
			++idx;
		}
	}

	Iter find(const Key &key) { return {this, lookup(key)}; }
	ConstIter find(const Key &key) const { return {this, lookup(key)}; }

	Maybe<Value> maybeFind(const Key &key) const {
		auto idx = lookup(key);
		return idx == m_capacity ? Maybe<Value>() : m_storage.value(idx);
	}

	void clear() {
		for(int n = 0; n < m_capacity; n++)
			if(!m_storage.isUnused(n)) {
				if(!m_storage.isDeleted(n))
					m_storage.destruct(n);
				m_storage.markUnused(n);
			}
		m_size = m_num_used = 0;
	}

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
	i64 usedMemory() const { return capacity() * Storage::memory_unit; }

	vector<Value> values() const {
		// TODO: some storages could return CSpan<>
		vector<Value> out;
		out.reserve(size());
		for(int n = 0; n < m_capacity; n++)
			if(m_storage.isValid(n))
				out.emplace_back(m_storage.value(n));
		return out;
	}

	vector<Key> keys() const {
		// TODO: some storages could return CSpan<>
		vector<Key> out;
		out.reserve(size());
		for(int n = 0; n < m_capacity; n++)
			if(m_storage.isValid(n))
				out.emplace_back(m_storage.key(n));
		return out;
	}

	auto hashes() const {
		static constexpr u32 unused_hash = 0xffffffff;
		static constexpr u32 deleted_hash = 0xfffffffe;

		if constexpr(keeps_hashes)
			return CSpan<u32>(m_storage.hashes, m_capacity);
		else {
			vector<Hash> out(m_capacity, unused_hash);
			for(int n = 0; n < m_capacity; n++) {
				if(m_storage.isDeleted(n))
					out[n] = deleted_hash;
				else if(m_storage.isValid(n))
					out[n] = hashFunc(m_storage.key(n));
			}
			return out;
		}
	}

	vector<Pair<Key, Value>> pairs() const { return transform<Pair<Key, Value>>(*this); }

	template <bool is_const> bool valid(TIter<is_const> iter) const {
		return iter.idx >= 0 && iter.idx <= m_capacity && iter.map == this;
	}

  private:
	static constexpr int initial_capacity = 64; // TODO: too big
	static_assert(isPowerOfTwo(initial_capacity));

	void grow() { grow(m_capacity == 0 ? initial_capacity : m_capacity * 2); }
	void grow(int new_capacity) {
		PASSERT(isPowerOfTwo(new_capacity));
		auto new_storage = Storage::allocate(new_capacity);

		rehash(new_capacity, new_storage, m_capacity, m_storage, true);
		m_storage.deallocate();

		m_capacity = new_capacity;
		m_capacity_mask = new_capacity - 1;
		m_used_limit = (int)(m_capacity * m_load_factor);
		m_storage = new_storage;
		m_num_used = m_size;
		PASSERT(m_num_used < m_capacity);
	}

	static Value defaultValue() {
		if constexpr(PolicyInfo::has_default_value)
			return Policy::defaultValue();
		else
			return Value();
	}

	Pair<Iter, bool> emplaceNew(int idx, const Key &key, Hash hash) {
		if(idx == m_capacity || m_num_used >= m_used_limit)
			return emplace(key, defaultValue());

		PASSERT(!m_storage.isValid(idx));
		if(m_storage.isUnused(idx))
			++m_num_used;
		m_storage.construct(idx, hash, key, defaultValue());
		++m_size;
		PASSERT(m_num_used >= m_size);
		return {{this, idx}, true};
	}

	int findForInsert(const Key &key, Hash hash) {
		int idx = hash & m_capacity_mask;
		if(m_storage.compareKey(idx, key, hash))
			return idx;
		if(idx == m_capacity)
			return m_capacity;

		int free_idx = m_storage.isDeleted(idx) ? idx : -1;

		// Guarantees loop termination.
		PASSERT(m_num_used < m_capacity);

		int num_probes = 1;
		while(!m_storage.isUnused(idx)) {
			idx = (idx + num_probes++) & m_capacity_mask;
			if(m_storage.compareKey(idx, key, hash))
				return idx;
			if(m_storage.isDeleted(idx) && free_idx == -1)
				free_idx = idx;
		}
		return free_idx != -1 ? free_idx : idx;
	}

	int lookup(const Key &key) const {
		auto hash = hashFunc(key);
		unsigned idx = hash & m_capacity_mask;
		if(m_storage.compareKey(idx, key, hash))
			return idx;

		// Guarantees loop termination.
		PASSERT(m_capacity == 0 || m_num_used < m_capacity);

		int num_probes = 1;
		while(!m_storage.isUnused(idx)) {
			idx = (idx + num_probes++) & m_capacity_mask;
			if(m_storage.compareKey(idx, key, hash))
				return idx;
		}
		return m_capacity;
	}

	void rehash(int new_capacity, Storage &new_storage, int capacity, Storage &old_storage,
				bool destruct_original) {
		const u32 mask = new_capacity - 1;
		for(int idx = 0; idx < capacity; idx++) {
			if(old_storage.isValid(idx)) {
				Hash hash;
				if constexpr(keeps_hashes)
					hash = old_storage.hashes[idx];
				else
					hash = hashFunc(old_storage.key(idx));

				u32 i = hash & mask;

				int num_probes = 1;
				while(!new_storage.isUnused(i))
					i = (i + num_probes++) & mask;
				new_storage.construct(i, hash, old_storage.key(idx), old_storage.value(idx));
				if(destruct_original)
					old_storage.destruct(idx);
			}
		}
	}

	void deleteNodes() {
		for(int n = 0; n < m_capacity; n++)
			if(m_storage.isValid(n))
				m_storage.destruct(n);
		m_storage.deallocate();
		m_capacity = m_size = m_num_used = m_capacity_mask = m_used_limit = 0;
	}

	void eraseNode(int idx) {
		PASSERT(m_storage.isValid(idx));
		m_storage.destruct(idx);
		m_storage.markDeleted(idx);
		--m_size;
	}

	Hash hashFunc(const Key &key) const {
		if constexpr(PolicyInfo::has_hash)
			return Policy::hash(key);
		else if constexpr(keeps_hashes)
			return hash<Hash>(key) & 0x7FFFFFFFu;
		else
			return hash<Hash>(key);
	}

	Storage m_storage;
	int m_size = 0, m_capacity = 0;
	int m_num_used = 0, m_used_limit = 0;
	float m_load_factor = 2.0f / 3.0f;
	u32 m_capacity_mask = 0;
	template <class HM, bool is_const> friend class HashMapIter;
};
}
