// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/hash.h"

namespace fwk {

// Original source: hash_map.h from RDE STL by Maciej Sinilo (Copyright 2007)
// Licensed under MIT license
//
// Slightly modified & adapted for libfwk by Krzysztof Jakubowski
template <typename TKey, typename TValue> class HashMap {
  public:
	using value_type = pair<TKey, TValue>;
	using uint32 = unsigned int;
	using hash_value_t = uint32;

  private:
	struct Node {
		// TODO: use single empty value; Use invalid<T>()
		// TODO: remove hash for small types
		static const hash_value_t kUnusedHash = 0xFFFFFFFF;
		static const hash_value_t kDeletedHash = 0xFFFFFFFE;

		Node() : hash(kUnusedHash) {}
		~Node() {}

		bool is_unused() const { return hash == kUnusedHash; }
		bool is_deleted() const { return hash == kDeletedHash; }
		bool is_occupied() const { return hash < kDeletedHash; }

		hash_value_t hash;
		union {
			value_type data;
		};
	};
	template <typename TNodePtr, typename TPtr, typename TRef> class node_iterator {
		friend class HashMap;

	  public:
		typedef std::forward_iterator_tag iterator_category;

		explicit node_iterator(TNodePtr node, const HashMap *map) : m_node(node), m_map(map) {}
		template <typename UNodePtr, typename UPtr, typename URef>
		node_iterator(const node_iterator<UNodePtr, UPtr, URef> &rhs)
			: m_node(rhs.node()), m_map(rhs.get_map()) {}

		TRef operator*() const {
			PASSERT(m_node);
			return m_node->data;
		}
		TPtr operator->() const { return &m_node->data; }
		TNodePtr node() const { return m_node; }

		node_iterator &operator++() {
			PASSERT(m_node);
			++m_node;
			move_to_next_occupied_node();
			return *this;
		}
		node_iterator operator++(int) {
			node_iterator copy(*this);
			++(*this);
			return copy;
		}

		bool operator==(const node_iterator &rhs) const { return rhs.m_node == m_node; }
		bool operator!=(const node_iterator &rhs) const { return !(rhs == *this); }

		const HashMap *get_map() const { return m_map; }

	  private:
		void move_to_next_occupied_node() {
			// @todo: save nodeEnd in constructor?
			TNodePtr nodeEnd = m_map->m_nodes + m_map->capacity();
			for(/**/; m_node < nodeEnd; ++m_node) {
				if(m_node->is_occupied())
					break;
			}
		}
		TNodePtr m_node;
		const HashMap *m_map;
	};

  public:
	typedef TKey key_type;
	typedef TValue mapped_type;
	typedef node_iterator<Node *, value_type *, value_type &> iterator;
	typedef node_iterator<const Node *, const value_type *, const value_type &> const_iterator;
	static const int kNodeSize = sizeof(Node);
	static const int kInitialCapacity = 64;
	static_assert((kInitialCapacity & (kInitialCapacity - 1)) == 0,
				  "Initial capacity must be a power of 2");

	// TODO: move constructors, etc.
	HashMap() {}
	explicit HashMap(int initial_bucket_count) { reserve(initial_bucket_count); }
	HashMap(const HashMap &rhs) { *this = rhs; }
	~HashMap() { deleteNodes(); }

	// Load factor controls hash map load. 4 means 100% load, ie. hashmap will grow
	// when number of items == capacity. Default value of 6 means it grows when
	// number of items == capacity * 3/2 (6/4). Higher load == tighter maps, but bigger
	// risk of collisions.
	void setLoadFactor4(int load_factor4) { m_load_factor4 = load_factor4; }
	int loadFactor4() const { return m_load_factor4; }

	iterator begin() {
		iterator it(m_nodes, this);
		it.move_to_next_occupied_node();
		return it;
	}
	const_iterator begin() const {
		const_iterator it(m_nodes, this);
		it.move_to_next_occupied_node();
		return it;
	}
	iterator end() { return iterator(m_nodes + m_capacity, this); }
	const_iterator end() const { return const_iterator(m_nodes + m_capacity, this); }

	mapped_type &operator[](const key_type &key) {
		hash_value_t hash = hashFunc(key);
		Node *n = find_for_insert(key, hash);
		if(n == 0 || !n->is_occupied())
			return emplace_at({key, TValue()}, n, hash).first->second;
		return n->data.second;
	}

	HashMap &operator=(const HashMap &rhs) {
		checkInvariant();
		if(&rhs != this) {
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
		}
		checkInvariant();
		return *this;
	}
	void swap(HashMap &rhs) {
		if(&rhs != this) {
			checkInvariant();
			swap(m_nodes, rhs.m_nodes);
			swap(m_size, rhs.m_size);
			swap(m_capacity, rhs.m_capacity);
			swap(m_capacity_mask, rhs.m_capacity_mask);
			swap(m_num_used, rhs.m_num_used);
			checkInvariant();
		}
	}

	pair<iterator, bool> emplace(const TKey &key, const TValue &value) {
		return emplace({key, value});
	}

	pair<iterator, bool> emplace(const value_type &v) {
		checkInvariant();
		if(m_num_used * m_load_factor4 >= m_capacity * 4)
			grow();

		hash_value_t hash = hashFunc(v.first);
		Node *n = find_for_insert(v.first, hash);
		if(n->is_occupied()) {
			DASSERT(hash == n->hash && v.first == n->data.first);
			return {iterator(n, this), false};
		}
		if(n->is_unused())
			++m_num_used;
		new(&n->data) value_type(v);
		n->hash = hash;
		++m_size;
		checkInvariant();
		return {iterator(n, this), true};
	}

	int erase(const key_type &key) {
		Node *n = lookup(key);
		if(n != (m_nodes + m_capacity) && n->is_occupied()) {
			eraseNode(n);
			return 1;
		}
		return 0;
	}
	void erase(iterator it) {
		DASSERT(it.get_map() == this);
		if(it != end()) {
			DASSERT(!empty());
			eraseNode(it.node());
		}
	}
	void erase(iterator from, iterator to) {
		for(/**/; from != to; ++from) {
			Node *n = from.node();
			if(n->is_occupied())
				eraseNode(n);
		}
	}

	iterator find(const key_type &key) {
		Node *n = lookup(key);
		return iterator(n, this);
	}
	const_iterator find(const key_type &key) const {
		const Node *n = lookup(key);
		return const_iterator(n, this);
	}

	void clear() {
		Node *endNode = m_nodes + m_capacity;
		for(Node *iter = m_nodes; iter != endNode; ++iter) {
			if(iter) {
				if(iter->is_occupied())
					iter->data.~value_type();
				// We can make them unused, because we clear whole hash_map,
				// so we can guarantee there'll be no holes.
				iter->hash = Node::kUnusedHash;
			}
		}
		m_size = 0;
		m_num_used = 0;
	}

	// TODO: something is wrong here (when reserving space in voxelizer/tri_points procedure
	// slows down drastically (slower the bigger reserve))
	void reserve(int min_size) {
		int newCapacity = (m_capacity == 0 ? kInitialCapacity : m_capacity);
		while(newCapacity < min_size)
			newCapacity *= 2;
		if(newCapacity > m_capacity)
			grow(newCapacity);
	}

	int capacity() const { return m_capacity; }
	int size() const { return m_size; }
	bool empty() const { return size() == 0; }

	int nonempty_bucket_count() const { return m_num_used; }
	int used_memory() const { return capacity() * kNodeSize; }

  private:
	void grow() {
		const int newCapacity = (m_capacity == 0 ? kInitialCapacity : m_capacity * 2);
		grow(newCapacity);
	}
	void grow(int new_capacity) {
		DASSERT((new_capacity & (new_capacity - 1)) == 0); // Must be power-of-two
		Node *newNodes = allocateNodes(new_capacity);
		rehash(new_capacity, newNodes, m_capacity, m_nodes, true);
		if(m_nodes != &s_empty_node)
			fwk::deallocate(m_nodes);
		m_capacity = new_capacity;
		m_capacity_mask = new_capacity - 1;
		m_nodes = newNodes;
		m_num_used = m_size;
		DASSERT(m_num_used < m_capacity);
	}
	pair<iterator, bool> emplace_at(const value_type &v, Node *n, hash_value_t hash) {
		checkInvariant();
		if(n == 0 || m_num_used * m_load_factor4 >= m_capacity * 4)
			return emplace(v);

		DASSERT(!n->is_occupied());
		if(n->is_unused())
			++m_num_used;
		new(&n->data) value_type(v);
		n->hash = hash;
		++m_size;
		checkInvariant();
		return {iterator(n, this), true};
	}
	Node *find_for_insert(const key_type &key, hash_value_t hash) {
		if(m_capacity == 0)
			return 0;

		uint32 i = hash & m_capacity_mask;

		Node *n = m_nodes + i;
		if(n->hash == hash && key == n->data.first)
			return n;

		Node *freeNode(0);
		if(n->is_deleted())
			freeNode = n;
		uint32 numProbes(0);
		// Guarantees loop termination.
		DASSERT(m_num_used < m_capacity);
		while(!n->is_unused()) {
			// TODO: try quadratic probing?
			++numProbes;
			i = (i + numProbes) & m_capacity_mask;
			n = m_nodes + i;
			if(compare_key(n, key, hash))
				return n;
			if(n->is_deleted() && freeNode == 0)
				freeNode = n;
		}
		return freeNode ? freeNode : n;
	}
	Node *lookup(const key_type &key) const {
		const hash_value_t hash = hashFunc(key);
		uint32 i = hash & m_capacity_mask;
		Node *n = m_nodes + i;
		if(n->hash == hash && key == n->data.first)
			return n;

		uint32 numProbes(0);
		// Guarantees loop termination.
		DASSERT(m_capacity == 0 || m_num_used < m_capacity);
		while(!n->is_unused()) {
			++numProbes;
			i = (i + numProbes) & m_capacity_mask;
			n = m_nodes + i;

			if(compare_key(n, key, hash))
				return n;
		}
		return m_nodes + m_capacity;
	}

	static void rehash(int new_capacity, Node *new_nodes, int capacity, const Node *nodes,
					   bool destruct_original) {
		//if (nodes == &s_empty_node || new_nodes == &s_empty_node)
		//  return;

		const Node *it = nodes;
		const Node *itEnd = nodes + capacity;
		const uint32 mask = new_capacity - 1;
		while(it != itEnd) {
			if(it->is_occupied()) {
				const hash_value_t hash = it->hash;
				uint32 i = hash & mask;

				Node *n = new_nodes + i;
				uint32 numProbes(0);
				while(!n->is_unused()) {
					++numProbes;
					i = (i + numProbes) & mask;
					n = new_nodes + i;
				}
				new(&n->data) value_type(it->data);
				n->hash = hash;
				if(destruct_original)
					it->data.~value_type();
			}
			++it;
		}
	}

	Node *allocateNodes(int n) {
		Node *buckets = static_cast<Node *>(fwk::allocate(n * sizeof(Node)));
		Node *iterBuckets(buckets);
		Node *end = iterBuckets + n;
		for(/**/; iterBuckets != end; ++iterBuckets)
			iterBuckets->hash = Node::kUnusedHash;

		return buckets;
	}
	void deleteNodes() {
		Node *it = m_nodes;
		Node *itEnd = it + m_capacity;
		while(it != itEnd) {
			if(it && it->is_occupied())
				it->data.~value_type();
			++it;
		}
		if(m_nodes != &s_empty_node)
			fwk::deallocate(m_nodes);

		m_capacity = 0;
		m_capacity_mask = 0;
		m_size = 0;
	}

	void eraseNode(Node *n) {
		DASSERT(!empty());
		DASSERT(n->is_occupied());
		n->data.~value_type();
		n->hash = Node::kDeletedHash;
		--m_size;
	}

	hash_value_t hashFunc(const key_type &key) const {
		const hash_value_t h = hash(key) & 0xFFFFFFFD;
		//DASSERT(h < node::kDeletedHash);
		return h;
	}
	void checkInvariant() const {
		PASSERT((m_capacity & (m_capacity - 1)) == 0);
		PASSERT(m_num_used >= m_size);
	}

	bool compare_key(const Node *n, const key_type &key, hash_value_t hash) const {
		return (n->hash == hash && key == n->data.first);
	}

	static Node s_empty_node;

	Node *m_nodes = &s_empty_node;
	int m_size = 0;
	int m_capacity = 0;
	uint32 m_capacity_mask = 0;
	int m_num_used = 0;
	int m_load_factor4 = 6;
};

template <typename TKey, typename TValue>
typename HashMap<TKey, TValue>::Node HashMap<TKey, TValue>::s_empty_node;
}
