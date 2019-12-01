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

  private:
	static constexpr HashValue unused_hash = 0xffffffff;
	static constexpr HashValue deleted_hash = 0xfffffffe;

	struct Node {
		Node() : hash(unused_hash) {}
		~Node() {}

		bool isUnused() const { return hash == unused_hash; }
		bool isDeleted() const { return hash == deleted_hash; }
		bool isOccupied() const { return hash < deleted_hash; }
		bool sameKey(const Key &key, HashValue key_hash) const {
			return key_hash == hash && key == data.first;
		}

		HashValue hash;
		union {
			KeyValuePair data;
		};
	};
	template <bool is_const> struct TIter {
		using TNodePtr = If<is_const, const Node *, Node *>;

		template <bool to_const, EnableIf<!is_const && to_const>...> operator TIter<to_const>() {
			return {ptr, end};
		}

		auto &operator*() const { return PASSERT(ptr != end), ptr->data; }
		auto *operator-> () const { return PASSERT(ptr != end), &ptr->data; }
		void operator++() { ++ptr, moveToNextOccupiedNode(); }
		bool operator==(const TIter &rhs) const { return rhs.ptr == ptr; }
		explicit operator bool() const { return ptr != end; }

		HashValue hash() const { return ptr->hash; }

		void moveToNextOccupiedNode() {
			while(ptr < end && ptr->hash >= deleted_hash)
				ptr++;
		}

		TNodePtr ptr, end;
	};

  public:
	using Iter = TIter<false>;
	using ConstIter = TIter<true>;
	static constexpr int initial_capacity = 64;
	static_assert(isPowerOfTwo(initial_capacity));

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
		Iter it{m_nodes, m_end};
		it.moveToNextOccupiedNode();
		return it;
	}
	ConstIter begin() const {
		ConstIter it{m_nodes, m_end};
		it.moveToNextOccupiedNode();
		return it;
	}

	Iter end() { return {m_end, m_end}; }
	ConstIter end() const { return {m_end, m_end}; }

	Value &operator[](const Key &key) {
		HashValue hash = hashFunc(key);
		Node *node = findForInsert(key, hash);
		if(!node || !node->isOccupied())
			return emplaceAt({key, TValue()}, node, hash).first->second;
		return node->data.second;
	}

	void operator=(const HashMap &rhs) {
		if(&rhs == this)
			return;

		clear();
		if(m_capacity < rhs.m_capacity) {
			deleteNodes();
			m_nodes = allocateNodes(rhs.m_capacity);
			m_capacity = rhs.m_capacity;
			m_capacity_mask = m_capacity - 1;
		}
		rehash(m_capacity, m_nodes, rhs.m_capacity, rhs.m_nodes, false);
		m_size = rhs.size();
		m_num_used = rhs.m_num_used;
		m_end = m_nodes + m_capacity;
		setLoadFactor(rhs.m_load_factor);
	}

	void swap(HashMap &rhs) {
		if(&rhs != this) {
			swap(m_nodes, rhs.m_nodes);
			swap(m_size, rhs.m_size);
			swap(m_capacity, rhs.m_capacity);
			swap(m_capacity_mask, rhs.m_capacity_mask);
			swap(m_end, rhs.m_end);
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
		Node *node = findForInsert(v.first, hash);
		if(node->isOccupied())
			return {{node, m_end}, false};
		if(node->isUnused())
			++m_num_used;
		new(&node->data) KeyValuePair(v);
		node->hash = hash;
		++m_size;
		PASSERT(m_num_used >= m_size);
		return {{node, m_end}, true};
	}

	bool erase(const Key &key) {
		Node *node = lookup(key);
		if(node != m_end && node->isOccupied()) {
			eraseNode(node);
			return true;
		}
		return false;
	}

	void erase(Iter it) {
		PASSERT(valid(it));
		if(it != end())
			eraseNode(it.ptr);
	}

	void erase(Iter from, Iter to) {
		PASSERT(valid(from) && valid(to));

		auto node = from.ptr;
		while(node != to.ptr) {
			if(node->isOccupied())
				eraseNode(node);
			++node;
		}
	}

	Iter find(const Key &key) { return {lookup(key), m_end}; }
	ConstIter find(const Key &key) const { return {lookup(key), m_end}; }
	Maybe<Value> maybeFind(const Key &key) const {
		auto it = find(key);
		if(it == end())
			return none;
		return it->second;
	}

	void clear() {
		for(int n = 0; n < m_capacity; n++) {
			if(m_nodes[n].isOccupied())
				m_nodes[n].data.~KeyValuePair();
			m_nodes[n].hash = unused_hash;
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
	int usedMemory() const { return capacity() * sizeof(Node); }

	Node *data() { return m_nodes; }
	const Node *data() const { return m_nodes; }

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
		return iter.ptr >= m_nodes && iter.end == m_end && iter.ptr <= iter.end;
	}

  private:
	void grow() { grow(m_capacity == 0 ? initial_capacity : m_capacity * 2); }
	void grow(int new_capacity) {
		PASSERT(isPowerOfTwo(new_capacity));
		Node *new_nodes = allocateNodes(new_capacity);
		rehash(new_capacity, new_nodes, m_capacity, m_nodes, true);
		if(m_nodes != &s_empty_node)
			fwk::deallocate(m_nodes);
		m_capacity = new_capacity;
		m_capacity_mask = new_capacity - 1;
		m_used_limit = (int)(m_capacity * m_load_factor);
		m_nodes = new_nodes;
		m_end = m_nodes + new_capacity;
		m_num_used = m_size;
		PASSERT(m_num_used < m_capacity);
	}

	Pair<Iter, bool> emplaceAt(const KeyValuePair &v, Node *node, HashValue hash) {
		if(!node || m_num_used >= m_used_limit)
			return emplace(v);

		PASSERT(!node->isOccupied());
		if(node->isUnused())
			++m_num_used;
		new(&node->data) KeyValuePair(v);
		node->hash = hash;
		++m_size;
		return {{node, m_end}, true};
	}
	Node *findForInsert(const Key &key, HashValue hash) {
		unsigned idx = hash & m_capacity_mask;
		Node *node = m_nodes + idx;
		if(node->sameKey(key, hash))
			return node;

		if(node == m_end)
			return nullptr;

		Node *free_node = node->hash == deleted_hash ? node : nullptr;

		// Guarantees loop termination.
		PASSERT(m_num_used < m_capacity);

		unsigned num_probes = 0;
		while(node->hash <= deleted_hash) {
			num_probes++;
			idx = (idx + num_probes) & m_capacity_mask;
			node = m_nodes + idx;
			if(node->sameKey(key, hash))
				return node;
			if(node->hash == deleted_hash && !free_node)
				free_node = node;
		}
		return free_node ? free_node : node;
	}

	Node *lookup(const Key &key) const {
		auto hash = hashFunc(key);
		unsigned idx = hash & m_capacity_mask;
		Node *node = m_nodes + idx;
		if(node->sameKey(key, hash))
			return node;

		// Guarantees loop termination.
		PASSERT(m_capacity == 0 || m_num_used < m_capacity);

		unsigned num_probes = 0;
		while(node->hash <= deleted_hash) {
			num_probes++;
			idx = (idx + num_probes) & m_capacity_mask;
			node = m_nodes + idx;
			if(node->sameKey(key, hash))
				return node;
		}
		return m_end;
	}

	static void rehash(int new_capacity, Node *new_nodes, int capacity, const Node *nodes,
					   bool destruct_original) {
		const Node *it = nodes;
		const Node *it_end = nodes + capacity;
		const u32 mask = new_capacity - 1;
		while(it != it_end) {
			if(it->isOccupied()) {
				const HashValue hash = it->hash;
				u32 i = hash & mask;

				Node *n = new_nodes + i;
				unsigned num_probes = 0;
				while(!n->isUnused()) {
					++num_probes;
					i = (i + num_probes) & mask;
					n = new_nodes + i;
				}
				new(&n->data) KeyValuePair(it->data);
				n->hash = hash;
				if(destruct_original)
					it->data.~KeyValuePair();
			}
			++it;
		}
	}

	Node *allocateNodes(int count) {
		Node *buckets = static_cast<Node *>(fwk::allocate(count * sizeof(Node)));
		for(int n = 0; n < count; n++)
			buckets[n].hash = unused_hash;
		return buckets;
	}

	void deleteNodes() {
		for(int n = 0; n < m_capacity; n++)
			if(m_nodes[n].isOccupied())
				m_nodes[n].data.~KeyValuePair();
		if(m_nodes != &s_empty_node)
			fwk::deallocate(m_nodes);
		m_capacity = m_size = m_capacity_mask = m_used_limit = 0;
	}

	void eraseNode(Node *n) {
		PASSERT(n->isOccupied());
		n->data.~KeyValuePair();
		n->hash = deleted_hash;
		--m_size;
	}

	HashValue hashFunc(const Key &key) const { return hash<HashValue>(key) & 0x7FFFFFFFu; }

	static Node s_empty_node;
	Node *m_nodes = &s_empty_node, *m_end = &s_empty_node;
	int m_size = 0, m_capacity = 0;
	int m_num_used = 0, m_used_limit = 0;
	float m_load_factor = 2.0f / 3.0f;
	u32 m_capacity_mask = 0;
};

template <typename TKey, typename TValue>
typename HashMap<TKey, TValue>::Node HashMap<TKey, TValue>::s_empty_node;
}
