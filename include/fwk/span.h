// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/range.h"

namespace fwk {

constexpr bool compatibleSizes(size_t a, size_t b) { return a > b ? a % b == 0 : b % a == 0; }

// Span is a range of elements spanning continuous range of memory (like vector, array, etc.).
// Span is very light-weight: it keeps a pointer and a size; User is responsible for making sure
//   that the container with values exist as long as Span exists.
template <class T, int min_size /*= 0*/> class Span {
  public:
	using value_type = RemoveConst<T>;
	enum { is_const = fwk::is_const<T>, minimum_size = min_size };
	static_assert(min_size >= 0, "min_size should be >= 0");

	Span(T *data, int size, NoAssertsTag) : m_data(data), m_size(size) {
		if(min_size > 0)
			DASSERT(m_size >= min_size);
	}
	Span(T *data, int size) : m_data(data), m_size(size) {
		DASSERT(m_data || m_size == 0);
		if(min_size > 0)
			DASSERT(m_size >= min_size);
	}

	Span() : m_data(nullptr), m_size(0) {
		static_assert(min_size == 0, "Cannot construct empty Span which has minimum_size > 0");
	}

	Span(T &single) : m_data(&single), m_size(1) {}
	Span(T *begin, T *end) : Span(begin, (int)(end - begin), no_asserts) { DASSERT(end >= begin); }
	template <int N> Span(value_type (&array)[N]) : m_data(array), m_size(N) {
		static_assert(N >= min_size);
	}
	template <int N> Span(const value_type (&array)[N]) : m_data(array), m_size(N) {
		static_assert(N >= min_size);
	}
	template <size_t N> Span(array<value_type, N> &arr) : m_data(arr.data()), m_size(N) {
		static_assert(N >= min_size);
	}
	template <size_t N> Span(const array<value_type, N> &arr) : m_data(arr.data()), m_size(N) {
		static_assert(N >= min_size);
	}
	Span(const Span &other) : m_data(other.m_data), m_size(other.m_size) {}
	Span &operator=(const Span &) = default;

	template <int rhs_min_size, EnableIf<rhs_min_size >= min_size>...>
	Span(Span<T, rhs_min_size> range) : m_data(range.data()), m_size(range.size()) {}

	template <class U = T, EnableIf<fwk::is_const<U>>...>
	Span(const Span<value_type, min_size> &range) : m_data(range.data()), m_size(range.size()) {}

	Span(const std::initializer_list<T> &list) : Span(list.begin(), list.end()) {}

	template <class TSpan, class Base = SpanBase<TSpan>,
			  EnableIf<is_same<Base, T> && !is_same<TSpan, Span>>...>
	Span(TSpan &rhs) : Span(fwk::data(rhs), fwk::size(rhs)) {}

	template <class TSpan, class Base = SpanBase<TSpan>,
			  EnableIf<is_same<Base, value_type> && !is_same<TSpan, Span> && is_const>...>
	Span(const TSpan &rhs) : Span(fwk::data(rhs), fwk::size(rhs)) {}

	const T *begin() const { return m_data; }
	const T *end() const { return m_data + m_size; }
	T *begin() { return m_data; }
	T *end() { return m_data + m_size; }

	const T &front() const {
		PASSERT(m_size > 0);
		return m_data[0];
	}
	const T &back() const {
		PASSERT(m_size > 0);
		return m_data[m_size - 1];
	}

	T *data() const { return m_data; }
	int size() const { return m_size; }
	bool inRange(int idx) const { return fwk::inRange(idx, 0, m_size); }

	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size > 0; }

	T &operator[](int idx) const {
		IF_PARANOID(checkInRange(idx, 0, m_size));
		return m_data[idx];
	}

	Span subSpan(int start) const {
		DASSERT(start >= 0 && start <= m_size);
		return {m_data + start, m_size - start};
	}

	Span subSpan(int start, int end) const {
		DASSERT(start >= 0 && start <= end);
		DASSERT(end <= m_size);
		return {m_data + start, end - start};
	}

	template <class U, EnableIf<is_same<RemoveConst<U>, value_type>>...>
	bool operator==(Span<U> rhs) const {
		if(m_size != rhs.size())
			return false;
		for(int n = 0; n < m_size; n++)
			if(m_data[n] != rhs[n])
				return false;
		return true;
	}

	template <class U, EnableIf<is_same<RemoveConst<U>, value_type>>...>
	bool operator<(Span<U> rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	template <class U> auto reinterpret() const {
		static_assert(compatibleSizes(sizeof(T), sizeof(U)),
					  "Incompatible sizes; are you sure, you want to do this cast?");
		using out_type = If<fwk::is_const<T>, const U, U>;
		auto new_size = size_t(m_size) * sizeof(T) / sizeof(U);
		return Span<out_type>(reinterpret_cast<out_type *>(m_data), new_size);
	}

  private:
	T *m_data;
	int m_size;
};

template <class T, int min_size = 0> using CSpan = Span<const T, min_size>;

template <class TSpan, class T = SpanBase<TSpan>> Span<T> span(TSpan &span) {
	return Span<T>(span);
}

template <class TSpan, class T = SpanBase<TSpan>> CSpan<T> cspan(const TSpan &span) {
	return CSpan<T>(span);
}

template <class T> Span<T> span(T *ptr, int size) { return Span<T>(ptr, size); }
template <class T> Span<T> span(T *begin, const T *end) { return Span<T>(begin, end); }
template <class T> CSpan<T> span(const std::initializer_list<T> &list) { return CSpan<T>(list); }

template <class T> CSpan<T> cspan(T *ptr, int size) { return CSpan<T>(ptr, size); }
template <class T> CSpan<T> cspan(T *begin, const T *end) { return CSpan<T>(begin, end); }
template <class T> CSpan<T> cspan(const std::initializer_list<T> &list) { return CSpan<T>(list); }

template <class TSpan, class T = SpanBase<TSpan>> Span<T> subSpan(TSpan &v, int start) {
	return span(v).subSpan(start);
}

template <class TSpan, class T = SpanBase<TSpan>> Span<T> subSpan(TSpan &v, int start, int end) {
	return span(v).subSpan(start, end);
}

template <class TSpan, class T, class TB = SpanBase<TSpan>, EnableIf<is_same<T, TB>>...>
int spanMemberIndex(const TSpan &span, const T &elem) {
	auto *ptr = &elem;
	PASSERT("Element is not a member of span" && ptr >= span.begin() && ptr < span.end());
	return ptr - span.begin();
}
}
