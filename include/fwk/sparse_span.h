// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/range.h"
#include "fwk/sparse_vector.h"

namespace fwk {

// Span for sparse data
template <class T> class SparseSpan {
  public:
	using value_type = T;
	enum { is_const = true };

	//TODO: non-const support ?
	//TODO: compaitibility with Span<>

	SparseSpan(const T *data, const bool *valids, int size, int end_index)
		: m_data(data), m_valids(valids), m_size(size), m_end_index(end_index) {
		PASSERT(size <= end_index);
	}
	SparseSpan(const T *data, CSpan<bool> valids, int size)
		: SparseSpan(data, valids.data(), size, valids.size()) {}

	template <class U = T, EnableIf<is_same<U, T>>...>
	SparseSpan(const SparseVector<U> &vec)
		: m_data(vec.rawData()), m_valids(vec.valids().data()), m_size(vec.size()),
		  m_end_index(vec.endIndex()) {
		static_assert(SparseVector<U>::compatible_alignment);
		static_assert(SparseVector<U>::same_size);
	}

	SparseSpan() : m_data(nullptr), m_valids(nullptr), m_end_index(0), m_size(0) {}

	int size() const { return m_size; }
	int endIndex() const { return m_end_index; }
	bool valid(int idx) const { return idx < m_end_index && m_valids[idx]; }

	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size > 0; }

	template <class Index = None> struct Iter {
		constexpr void operator++() {
			idx++;
			// TODO: use sentinel
			while(idx < span.m_end_index && !span.m_valids[idx])
				idx++;
		}

		using Ret = If<is_same<Index, None>, const T &, Index>;
		Ret operator*() const {
			if constexpr(is_same<Index, None>)
				return span.m_data[idx];
			else
				return Ret(idx);
		}
		constexpr bool operator!=(const Iter &rhs) const { return idx != rhs.idx; }

		const SparseSpan &span;
		int idx;
	};

	template <class U> struct Indices {
		auto begin() const { return Iter<U>{span, span.firstIndex()}; }
		auto end() const { return Iter<U>{span, span.m_end_index}; }
		const SparseSpan &span;
	};

	int firstIndex() const {
		PASSERT(!empty());
		int index = 0;
		while(index < m_end_index && !m_valids[index])
			index++;
		return index;
	}

	int lastIndex() const {
		PASSERT(!empty());
		int index = m_end_index - 1;
		while(index >= 0 && !m_valids[index])
			--index;
		return index;
	}

	int nextIndex(int idx) const {
		idx++;
		while(idx < m_end_index && !m_valids[idx])
			idx++;
		return idx;
	}

	const T &front() const { return m_data[firstIndex()]; }
	const T &back() const { return m_data[lastIndex()]; }

	Iter<None> begin() const { return {*this, firstIndex()}; }
	Iter<None> end() const { return {*this, m_end_index}; }
	template <class Idx = int> auto indices() const { return Indices<Idx>{*this}; }

	const T &operator[](int idx) const {
		PASSERT(valid(idx));
		return m_data[idx];
	}

  private:
	const T *m_data;
	const bool *m_valids;
	int m_size, m_end_index;
};
}
