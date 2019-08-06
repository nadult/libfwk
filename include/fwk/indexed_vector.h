// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/index_range.h"
#include "fwk/list_node.h"
#include "fwk/pod_vector.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: Better name ?
// Constant time emplace & erase.
// All elements are in single continuous block of memory, but:
// there may be holes between valid elements.
template <class T> class IndexedVector {
	union Element;

  public:
	static constexpr int initial_size = 8;
	// TODO: use special list node with same alignment as T?
	static constexpr bool compatible_alignment = alignof(T) == alignof(Element);
	static constexpr bool same_size = sizeof(T) == sizeof(Element);

	// Use only when alignment and size match
	T *rawData() { return reinterpret_cast<T *>(m_elements.data()); }
	const T *rawData() const { return reinterpret_cast<const T *>(m_elements.data()); }

	IndexedVector() {}
	~IndexedVector() {
		for(int n = 0; n < m_elements.size(); n++)
			m_elements[n].free(m_valids[n]);
	}

	IndexedVector(const IndexedVector &rhs)
		: m_valids(rhs.m_valids), m_free_list(rhs.m_free_list), m_valid_count(rhs.m_valid_count),
		  m_end_index(rhs.m_end_index) {
		m_elements.resize(rhs.m_elements.size());
		for(int n = 0; n < m_end_index; n++)
			new(&m_elements[n]) Element(rhs.m_elements[n], m_valids[n]);
	}

	IndexedVector(IndexedVector &&rhs)
		: m_elements(move(rhs.m_elements)), m_valids(move(rhs.m_valids)),
		  m_free_list(rhs.m_free_list), m_valid_count(rhs.m_valid_count),
		  m_end_index(rhs.m_end_index) {
		rhs.m_free_list = List();
		rhs.m_valid_count = 0;
		rhs.m_end_index = 0;
	}

	IndexedVector(vector<T> &&vec)
		: m_valids(vec.size(), true), m_valid_count(vec.size()), m_end_index(vec.size()) {
		if constexpr(sizeof(T) == sizeof(Element)) {
			auto temp = vec.template reinterpret<Element>();
			m_elements.unsafeSwap(temp);
		} else {
			m_elements.resize(vec.size());
			for(int n = 0; n < vec.size(); n++)
				m_elements[n].value = move(vec[n]);
		}
	}

	void swap(IndexedVector &rhs) {
		m_elements.swap(rhs.m_elements);
		m_valids.swap(rhs.m_valids);
		fwk::swap(m_free_list, rhs.m_free_list);
		fwk::swap(m_valid_count, rhs.m_valid_count);
		fwk::swap(m_end_index, rhs.m_end_index);
	}

	void operator=(const IndexedVector &rhs) {
		if(this == &rhs)
			return;
		IndexedVector copy(rhs);
		swap(copy);
	}

	void operator=(IndexedVector &&rhs) {
		clear();
		swap(rhs);
	}

	T &operator[](int idx) {
		PASSERT(valid(idx));
		return m_elements[idx].value;
	}
	const T &operator[](int idx) const {
		PASSERT(valid(idx));
		return m_elements[idx].value;
	}

	int size() const { return m_valid_count; }
	int capacity() const { return m_elements.size(); }
	bool empty() const { return m_valid_count == 0; }

	void clear() {
		for(int n = 0; n < m_elements.size(); n++)
			m_elements[n].free(m_valids[n]);
		m_elements.clear();
		m_valids.clear();
		m_free_list = List();
		m_valid_count = 0;
		m_end_index = 0;
	}

	void reserve(int size) { reallocate(insertCapacity(size)); }
	bool valid(int index) const { return index >= 0 && index < m_end_index && m_valids[index]; }

	template <class... Args> int emplace(Args &&... args) {
		int index = alloc();
		new(&m_elements[index].value) T{std::forward<Args>(args)...};
		m_valids[index] = true;
		m_valid_count++;
		return index;
	}

	// If there is an element at given index, it will be destroyed
	template <class... Args> void emplaceAt(int index, Args &&... args) {
		if(valid(index))
			erase(index);

		if(index >= capacity())
			reallocate(insertCapacity(index + 1));

		while(m_end_index <= index) {
			m_elements[m_end_index].node = ListNode();
			listInsert<Element, &Element::node>(m_elements, m_free_list, m_end_index++);
		}

		listRemove<Element, &Element::node>(m_elements, m_free_list, index);
		new(&m_elements[index].value) T{std::forward<Args>(args)...};
		m_valids[index] = true;
		m_valid_count++;
	}

	// TODO: can we iterate & erase safely in a loop?
	void erase(int index) {
		DASSERT(valid(index));
		m_elements[index].free(true);
		m_elements[index].node = ListNode();
		m_valids[index] = false;
		listInsert<Element, &Element::node>(m_elements, m_free_list, index);
		m_valid_count--;
	}

	int firstIndex() const {
		int index = 0;
		while(index < m_end_index && !valid(index))
			index++;
		return index;
	}

	int nextIndex(int idx) const {
		idx++;
		while(idx < m_end_index && !m_valids[idx])
			idx++;
		return idx;
	}

	int endIndex() const { return m_end_index; }
	int nextFreeIndex() const { return m_free_list.empty() ? m_end_index : m_free_list.head; }

	bool growForNext() {
		if(m_free_list.empty() && m_end_index == m_elements.size()) {
			grow();
			return true;
		}
		return false;
	}

	template <bool is_const> struct Iter {
		using Vec = typename std::conditional<is_const, const IndexedVector, IndexedVector>::type;
		Iter(int index, int max_index, Vec &vector)
			: index(index), max_index(max_index), vector(vector) {}

		const Iter &operator++() {
			index++;
			while(index < max_index && !vector.valid(index))
				index++;
			return *this;
		}
		auto &operator*() const { return vector[index]; }
		bool operator==(const Iter &rhs) const { return index == rhs.index; }
		bool operator<(const Iter &rhs) const { return index < rhs.index; }

	  private:
		int index, max_index;
		Vec &vector;
	};

	auto begin() { return Iter<false>(firstIndex(), m_end_index, *this); }
	auto end() { return Iter<false>(m_end_index, m_end_index, *this); }

	auto begin() const { return Iter<true>(firstIndex(), m_end_index, *this); }
	auto end() const { return Iter<true>(m_end_index, m_end_index, *this); }

	template <class Idx = int> auto indices() const {
		const bool *valids = m_valids.data();
		return IndexRange(
			firstIndex(), m_end_index, [](int idx) { return Idx(idx); },
			[=](int idx) { return valids[idx]; });
	}

	bool operator==(const IndexedVector &rhs) const {
		return m_valid_count == rhs.m_valid_count && compare(rhs) == 0;
	}

	bool operator<(const IndexedVector &rhs) const { return compare(rhs) == -1; }

	int compare(const IndexedVector &rhs) const {
		if(m_end_index < rhs.m_end_index)
			return -rhs.compare(*this);

		int min_index = min(m_end_index, rhs.m_end_index);
		for(int n = 0; n < min_index; n++) {
			bool is_valid = m_valids[n];
			if(is_valid != rhs.m_valids[n])
				return is_valid ? 1 : -1;
			if(is_valid && !(m_elements[n].value == rhs.m_elements[n].value))
				return m_elements[n].value < rhs.m_elements[n].value ? -1 : 1;
		}
		for(int n = min_index; n < m_end_index; n++)
			if(m_valids[n])
				return 1;
		return 0;
	}

	CSpan<bool> valids() const { return m_valids; }

	int growCapacity() const {
		int capacity = m_elements.size();
		return capacity > 4096 ? capacity * 2 : max((capacity * 3 + 1) / 2, initial_size);
	}

	int insertCapacity(int min_size) const {
		int cap = growCapacity();
		return cap > min_size ? cap : min_size;
	}

	int indexOf(const T &object) const {
		auto *ptr = reinterpret_cast<const Element *>(&object);
		PASSERT("Invalid alignment" && u64(ptr) % alignof(Element) == 0);
		auto idx = spanMemberIndex(m_elements, *ptr);
		PASSERT(m_valids[idx]);
		return idx;
	}

  private:
	union Element {
		Element() {}
		Element(const Element &rhs, bool is_init) {
			if(is_init)
				new(&value) T(rhs.value);
			else
				new(&node) ListNode(rhs.node);
		}
		Element(Element &&rhs, bool is_init) {
			if(is_init)
				new(&value) T(std::move(rhs.value));
			else
				new(&node) ListNode(rhs.node);
		}

		void free(bool is_init) {
			if(is_init)
				value.~T();
		}
		int compare(const Element &rhs, bool is_init, bool rhs_is_init) const { return 0; }

		T value;
		ListNode node;
	};

	void reallocate(int new_capacity) {
		if(new_capacity <= capacity())
			return;
		PodVector<Element> new_elems(new_capacity);
		m_valids.resize(new_capacity);
		for(int n = 0; n < m_elements.size(); n++) {
			new(&new_elems[n]) Element(std::move(m_elements[n]), m_valids[n]);
			m_elements[n].free(m_valids[n]);
		}
		m_elements.swap(new_elems);
	}

	void grow() { reallocate(growCapacity()); };

	int alloc() {
		if(m_free_list.empty()) {
			if(m_end_index == m_elements.size())
				grow();
			return m_end_index++;
		}
		int idx = m_free_list.head;
		listRemove<Element, &Element::node>(m_elements, m_free_list, idx);
		return idx;
	}

	PodVector<Element> m_elements;
	vector<bool> m_valids;
	// TODO: skip list instead of valid list?
	List m_free_list;
	int m_valid_count = 0;
	int m_end_index = 0;
};
}
