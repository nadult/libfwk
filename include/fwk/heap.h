// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/pod_vector.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class T> class Heap {
  public:
	static_assert(std::is_pod<T>::value, "T should be POD");

	Heap(int max_keys) : m_heap(max_keys), m_indices(max_keys), m_size(0) {
		for(auto &i : m_indices)
			i = -1;
	}

	int maxSize() const { return (int)m_heap.size(); }
	int size() const { return m_size; }
	bool empty() const { return m_size == 0; }

	Pair<T, int> extractMin() {
		DASSERT(m_size > 0);
		auto min = m_heap[0];
		m_indices[min.second] = -1;
		if(m_size > 1) {
			m_heap[0] = m_heap[--m_size];
			updateIndex(0);
			heapify(0);
		} else
			m_size = 0;
		return min;
	}

	void insert(int key_idx, T value) {
		DASSERT(key_idx >= 0 && key_idx < maxSize());
		DASSERT(m_indices[key_idx] == -1);
		update(key_idx, value);
	}

	T value(int key_idx) const {
		DASSERT(key_idx >= 0 && key_idx < maxSize());
		DASSERT(m_indices[key_idx] != -1);
		return m_heap[m_indices[key_idx]].first;
	}

	// TODO: this sux, fix it
	void update(int key_idx, T value) {
		DASSERT(key_idx >= 0 && key_idx < maxSize());

		int pos = m_indices[key_idx];
		if(pos == -1) {
			pos = m_size++;
			m_heap[pos].first = value;
		}

		if(value > m_heap[pos].first) {
			m_heap[pos].first = value;
			heapify(pos);
			return;
		}

		while(pos > 0 && value < m_heap[pos / 2].first) {
			m_heap[pos] = m_heap[pos / 2];
			updateIndex(pos);
			pos = pos / 2;
		}
		m_heap[pos] = {value, key_idx};
		updateIndex(pos);
	}

	bool invariant() const {
		for(int n = 1; n < m_size; n++)
			if(less(n, n / 2))
				return false;
		return true;
	}

	auto &operator[](int idx) const { return m_heap[idx]; }
	int index(int vidx) const { return m_indices[vidx]; }

  private:
	bool less(int a, int b) const { return m_heap[a].first < m_heap[b].first; }
	void updateIndex(int pos) { m_indices[m_heap[pos].second] = pos; }
	void heapify(int pos) {
		int l = pos * 2;
		int r = pos * 2 + 1;

		int smallest = l < m_size && less(l, pos) ? l : pos;
		if(r < m_size && less(r, smallest))
			smallest = r;
		if(smallest != pos) {
			swap(m_heap[pos], m_heap[smallest]);
			updateIndex(pos);
			updateIndex(smallest);
			heapify(smallest);
		}
	}

	PodVector<pair<T, int>> m_heap;
	PodVector<int> m_indices;
	int m_size;
};

}
