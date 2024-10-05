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

	SparseSpan(const T *data, const bool *valids, int size, int spread)
		: m_data(data), m_valids(valids), m_size(size), m_spread(spread) {
		PASSERT(size <= spread);
	}
	SparseSpan(const T *data, CSpan<bool> valids, int size)
		: SparseSpan(data, valids.data(), size, valids.size()) {}
	SparseSpan(CSpan<T> span)
		: m_data(span.data()), m_valids(nullptr), m_size(span.size()), m_spread(span.size()) {}
	template <class USpan, class U = RemoveConst<SpanBase<USpan>>>
		requires(is_same<T, U>)
	SparseSpan(const USpan &span) : SparseSpan(cspan(span)) {}

	template <class U = T>
		requires(is_same<U, T>)
	SparseSpan(const SparseVector<U> &vec)
		: m_data(vec.rawData()), m_valids(vec.valids().data()), m_size(vec.size()),
		  m_spread(vec.spread()) {
		static_assert(SparseVector<U>::compatible_alignment);
		static_assert(SparseVector<U>::same_size);
	}

	SparseSpan() : m_data(nullptr), m_valids(nullptr), m_size(0), m_spread(0) {}

	int size() const { return m_size; }
	int spread() const { return m_spread; }
	bool valid(int idx) const { return idx < m_spread && (!m_valids || m_valids[idx]); }

	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size > 0; }

	int firstIndex() const {
		int idx = 0;
		if(m_valids)
			while(idx < m_spread && !m_valids[idx])
				idx++;
		return idx;
	}
	int lastIndex() const {
		if(m_size == 0)
			return m_spread;
		int idx = m_spread - 1;
		if(m_valids)
			while(idx >= 0 && !m_valids[idx])
				--idx;
		return idx;
	}
	int nextIndex(int idx) const {
		idx++;
		if(m_valids)
			while(idx < m_spread && !m_valids[idx])
				idx++;
		return idx; // TODO: clamp to spread ?
	}

	const T &front() const { return PASSERT(!empty()), m_data[firstIndex()]; }
	const T &back() const { return PASSERT(!empty()), m_data[lastIndex()]; }

	Iter<None> begin() const { return {*this, firstIndex()}; }
	Iter<None> end() const { return {*this, m_spread}; }
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
		constexpr bool operator==(const Iter &rhs) const { return idx == rhs.idx; }

		const SparseSpan &span;
		int idx;
	};

	template <class U> struct Indices {
		auto begin() const { return Iter<U>{span, span.firstIndex()}; }
		auto end() const { return Iter<U>{span, span.m_spread}; }
		const SparseSpan &span;
	};

  private:
	const T *m_data;
	const bool *m_valids;
	int m_size, m_spread;
};
}
