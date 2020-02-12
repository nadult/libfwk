// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"

namespace fwk {

template <class T, int N> struct Array {
	using Type = T;
	static constexpr int size_ = N;

	void swap(Array &other) { std::swap_ranges(begin(), end(), other.begin()); }

	const T *data() const { return m_data; }
	const T *begin() const { return m_data; }
	const T *end() const { return m_data + N; }

	T *data() { return m_data; }
	T *begin() { return m_data; }
	T *end() { return m_data + N; }

	const T &front() const { return m_data[0]; }
	const T &back() const { return m_data[N - 1]; }

	T &front() { return m_data[0]; }
	T &back() { return m_data[N - 1]; }

	int size() const { return N; }
	bool empty() const { return N == 0; }

	const T &operator[](int idx) const {
		IF_PARANOID(checkInRange(idx, 0, N));
		return m_data[idx];
	}
	T &operator[](int idx) {
		IF_PARANOID(checkInRange(idx, 0, N));
		return m_data[idx];
	}

	inline bool operator==(const Array &rhs) const {
		return std::equal(begin(), end(), rhs.begin());
	}

	inline bool operator<(const Array &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	template <int I> T &get() { return m_data[I]; }
	template <int I> const T &get() const { return m_data[I]; }

	T m_data[N];
};
}

// Deduction guides:
//template <class T, class... U, EnableIf<(is_same<T, I> && ...)>...>
//Array(T, U...)->Array<T, 1 + sizeof...(U)>;

namespace std {
template <class> struct tuple_size;
template <std::size_t, class> struct tuple_element;
template <class T, int N>
struct tuple_size<fwk::Array<T, N>> : public integral_constant<std::size_t, N> {};

template <std::size_t I, class T, int N> struct tuple_element<I, fwk::Array<T, N>> {
	static_assert(I < N, "index is out of bounds");
	using type = T;
};
}
