// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/range.h"
#include "fwk/sparse_vector.h"

namespace fwk {

// Span for sparse data.
// Normal spans can be converted to sparse spans.
//
// TODO: non-const support ?
template <class T> class SparseSpan {
  public:
	template <class Index> struct Iter;
	using value_type = T;
	enum { is_const = true };

	SparseSpan(const T *data, const bool *valids, int size, int end_index)
		: m_data(data), m_valids(valids), m_size(size), m_end_index(end_index) {
		PASSERT(size <= end_index);
	}
	SparseSpan(const T *data, CSpan<bool> valids, int size)
		: SparseSpan(data, valids.data(), size, valids.size()) {}
	SparseSpan(CSpan<T> span)
		: m_data(span.data()), m_valids(nullptr), m_size(span.size()), m_end_index(span.size()) {}
	template <class USpan, class U = RemoveConst<SpanBase<USpan>>, EnableIf<is_same<T, U>>...>
	SparseSpan(const USpan &span) : SparseSpan(cspan(span)) {}

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
	bool valid(int idx) const { return idx < m_end_index && (!m_valids || m_valids[idx]); }

	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size > 0; }

	int firstIndex() const {
		int idx = 0;
		if(m_valids)
			while(idx < m_end_index && !m_valids[idx])
				idx++;
		return idx;
	}
	int lastIndex() const {
		if(m_size == 0)
			return m_end_index;
		int idx = m_end_index - 1;
		if(m_valids)
			while(idx >= 0 && !m_valids[idx])
				--idx;
		return idx;
	}
	int nextIndex(int idx) const {
		idx++;
		if(m_valids)
			while(idx < m_end_index && !m_valids[idx])
				idx++;
		return idx; // TODO: clamp to end_index ?
	}

	const T &front() const { return PASSERT(!empty()), m_data[firstIndex()]; }
	const T &back() const { return PASSERT(!empty()), m_data[lastIndex()]; }

	Iter<None> begin() const { return {*this, firstIndex()}; }
	Iter<None> end() const { return {*this, m_end_index}; }
	template <class Idx = int> auto indices() const { return Indices<Idx>{*this}; }

	const T &operator[](int idx) const {
		PASSERT(valid(idx));
		return m_data[idx];
	}

	template <class Index> struct Iter {
		constexpr void operator++() { idx = span.nextIndex(idx); }

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

  private:
	const T *m_data;
	const bool *m_valids;
	int m_size, m_end_index;
};
}
