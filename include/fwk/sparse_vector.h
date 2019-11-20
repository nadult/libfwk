// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/list_node.h"
#include "fwk/pod_vector.h"
#include "fwk/vector.h"

namespace fwk {

namespace detail {
	[[noreturn]] void invalidIndexSparse(int idx, int spread);
}

// Constant time emplace & erase.
// All elements are in single continuous block of memory,
// but there may be holes between valid elements.
// Spread defines range for element indices.
//
// TODO(opt): use sentinel at end-index ? problem: valids would have to be hidden
//            because they would lie about after-last element?
// TODO(opt): using bits instead of booleans probably doesn't make sense; Iteration would
//            be slower in the general case (only in very sparse arrays it would improve perf
//            but then in such case it would be better to compact such an array)
template <class T> class SparseVector {
	union Element;
#define LIST_ACCESSOR [&](int idx) -> ListNode & { return m_elements[idx].node; }

  public:
	static constexpr int initial_size = 8;
	// TODO: use special list node with same alignment as T?
	static constexpr bool compatible_alignment = alignof(T) == alignof(Element);
	static constexpr bool same_size = sizeof(T) == sizeof(Element);

	// Use only when alignment and size match
	T *rawData() { return reinterpret_cast<T *>(m_elements.data()); }
	const T *rawData() const { return reinterpret_cast<const T *>(m_elements.data()); }

	SparseVector() {}
	~SparseVector() {
		for(int n = 0; n < m_elements.size(); n++)
			m_elements[n].free(m_valids[n]);
	}

	SparseVector(const SparseVector &rhs)
		: m_valids(rhs.m_valids), m_free_list(rhs.m_free_list), m_size(rhs.m_size),
		  m_spread(rhs.m_spread) {
		m_elements.resize(rhs.m_elements.size());
		for(int n = 0; n < m_spread; n++)
			new(&m_elements[n]) Element(rhs.m_elements[n], m_valids[n]);
	}

	SparseVector(SparseVector &&rhs)
		: m_elements(move(rhs.m_elements)), m_valids(move(rhs.m_valids)),
		  m_free_list(rhs.m_free_list), m_size(rhs.m_size), m_spread(rhs.m_spread) {
		rhs.m_free_list = List();
		rhs.m_size = 0;
		rhs.m_spread = 0;
	}

	SparseVector(vector<T> &&vec)
		: m_valids(vec.size(), true), m_size(vec.size()), m_spread(vec.size()) {
		if constexpr(sizeof(T) == sizeof(Element)) {
			auto temp = vec.template reinterpret<Element>();
			m_elements.unsafeSwap(temp);
		} else {
			m_elements.resize(vec.size());
			for(int n = 0; n < vec.size(); n++)
				m_elements[n].value = move(vec[n]);
		}
	}

	void swap(SparseVector &rhs) {
		m_elements.swap(rhs.m_elements);
		m_valids.swap(rhs.m_valids);
		fwk::swap(m_free_list, rhs.m_free_list);
		fwk::swap(m_size, rhs.m_size);
		fwk::swap(m_spread, rhs.m_spread);
	}

	void operator=(const SparseVector &rhs) {
		if(this == &rhs)
			return;
		SparseVector copy(rhs);
		swap(copy);
	}

	void operator=(SparseVector &&rhs) {
		clear();
		swap(rhs);
	}

	T &operator[](int idx) {
		IF_PARANOID(checkIndex(idx));
		return m_elements[idx].value;
	}
	const T &operator[](int idx) const {
		IF_PARANOID(checkIndex(idx));
		return m_elements[idx].value;
	}

	int size() const { return m_size; }
	int capacity() const { return m_elements.size(); }
	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size > 0; }

	void clear() {
		for(int n = 0; n < m_elements.size(); n++)
			m_elements[n].free(m_valids[n]);
		m_elements.clear();
		m_valids.clear();
		m_free_list = List();
		m_size = 0;
		m_spread = 0;
	}

	void reserve(int size) { reallocate(insertCapacity(size)); }
	bool valid(int index) const { return index >= 0 && index < m_spread && m_valids[index]; }

	template <class... Args> int emplace(Args &&... args) {
		int index = alloc();
		new(&m_elements[index].value) T{std::forward<Args>(args)...};
		m_valids[index] = true;
		m_size++;
		return index;
	}

	// If there is an element at given index, it will be destroyed
	template <class... Args> void emplaceAt(int index, Args &&... args) {
		if(valid(index))
			erase(index);

		if(index >= capacity())
			reallocate(insertCapacity(index + 1));

		auto acc = [&](int idx) -> ListNode & { return m_elements[idx].node; };
		while(m_spread <= index) {
			m_elements[m_spread].node = ListNode();
			listInsert(LIST_ACCESSOR, m_free_list, m_spread++);
		}

		listRemove(LIST_ACCESSOR, m_free_list, index);
		new(&m_elements[index].value) T{std::forward<Args>(args)...};
		m_valids[index] = true;
		m_size++;
	}

	// TODO: can we iterate & erase safely in a loop?
	void erase(int index) {
		DASSERT(valid(index));
		m_elements[index].free(true);
		m_elements[index].node = ListNode();
		m_valids[index] = false;
		listInsert(LIST_ACCESSOR, m_free_list, index);
		m_size--;
	}

	int firstIndex() const {
		int idx = 0;
		while(idx < m_spread && !m_valids[idx])
			idx++;
		return idx;
	}
	int lastIndex() const {
		if(m_size == 0)
			return m_spread;
		int idx = m_spread - 1;
		while(idx >= 0 && !m_valids[idx])
			--idx;
		return idx;
	}
	int nextIndex(int idx) const {
		idx++;
		while(idx < m_spread && !m_valids[idx])
			idx++;
		return idx; // TODO: clamp to spread ?
	}

	int spread() const { return m_spread; }
	int nextFreeIndex() const { return m_free_list.empty() ? m_spread : m_free_list.head; }

	bool growForNext() {
		if(m_free_list.empty() && m_spread == m_elements.size()) {
			grow();
			return true;
		}
		return false;
	}

	template <bool is_const, class Index = None> struct Iter {
		using Ret = If<is_same<Index, None>, If<is_const, const T &, T &>, Index>;
		using Vec = If<is_const, const SparseVector, SparseVector>;

		Ret operator*() const {
			if constexpr(is_same<Index, None>)
				return vec[idx];
			else
				return Ret(idx);
		}
		constexpr void operator++() { idx = vec.nextIndex(idx); }
		constexpr bool operator==(const Iter &rhs) const { return idx == rhs.idx; }

		int idx;
		Vec &vec;
	};

	template <class U> struct Indices {
		auto begin() const { return Iter<true, U>{vec.firstIndex(), vec}; }
		auto end() const { return Iter<true, U>{vec.m_spread, vec}; }
		int size() const { return vec.m_size; }
		const SparseVector &vec;
	};

	auto begin() { return Iter<false>{firstIndex(), *this}; }
	auto end() { return Iter<false>{m_spread, *this}; }

	auto begin() const { return Iter<true>{firstIndex(), *this}; }
	auto end() const { return Iter<true>{m_spread, *this}; }

	template <class Idx = int> auto indices() const { return Indices<Idx>{*this}; }

	bool operator==(const SparseVector &rhs) const {
		return m_size == rhs.m_size && compare(rhs) == 0;
	}

	bool operator<(const SparseVector &rhs) const { return compare(rhs) == -1; }

	int compare(const SparseVector &rhs) const {
		if(m_spread < rhs.m_spread)
			return -rhs.compare(*this);

		int min_index = min(m_spread, rhs.m_spread);
		for(int n = 0; n < min_index; n++) {
			bool is_valid = m_valids[n];
			if(is_valid != rhs.m_valids[n])
				return is_valid ? 1 : -1;
			if(is_valid && !(m_elements[n].value == rhs.m_elements[n].value))
				return m_elements[n].value < rhs.m_elements[n].value ? -1 : 1;
		}
		for(int n = min_index; n < m_spread; n++)
			if(m_valids[n])
				return 1;
		return 0;
	}

	CSpan<bool> valids() const { return CSpan<bool>(m_valids.data(), m_spread); }

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
	void checkIndex(int idx) const {
		if(!m_valids[idx])
			detail::invalidIndexSparse(idx, m_spread);
	}

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
			if(m_spread == m_elements.size())
				grow();
			return m_spread++;
		}
		int idx = m_free_list.head;
		listRemove(LIST_ACCESSOR, m_free_list, idx);
		return idx;
	}

#undef LIST_ACCESSOR

	PodVector<Element> m_elements;
	vector<bool> m_valids;
	// TODO: skip list instead of valid list?
	List m_free_list;
	int m_size = 0;
	int m_spread = 0;
};
}
