// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_range.h"

namespace fwk {

// Examples:
//
// intRange(4):            0, 1, 2, 3
// intRange(10, 15):       10, 11, 12, 13, 14
// indexRange<T>(4):       T(0), T(1), T(2), T(3)
// wrappedPairsRange(4):   (0, 1), (1, 2), (2, 3), (3, 0)
// wrappedTriplesRange(3): (0, 1, 2), (1, 2, 0), (2, 0, 1)
//
// ??Range( some_range ) -> ??Range(0, size(some_range))

// IndexRange has to exist as long as any of the iterators belonging to it
template <class Func> class IndexRange {
  public:
	using Value = decltype(DECLVAL(Func)(0));
	IndexRange(int start, int end, Func func) : it_start(start), it_end(end), func(move(func)) {
		DASSERT(start <= end);
	}

	class Iter {
	  public:
		using difference_type = int;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = Value;
		using reference = value_type &;
		using pointer = value_type *;

		constexpr Iter(int index, const Func &func) : index(index), func(func) {}

		constexpr const Iter &operator++() {
			index++;
			return *this;
		}
		Value operator*() const { return func(index); }
		constexpr int operator-(Iter it) const { return index - it.index; }

		constexpr bool operator!=(const Iter &rhs) const { return index != rhs.index; }
		constexpr bool operator==(const Iter &rhs) const { return index == rhs.index; }
		constexpr bool operator<(const Iter &rhs) const { return index < rhs.index; }

	  private:
		const Func &func;
		int index;
	};

	auto begin() const { return Iter(it_start, func); }
	auto end() const { return Iter(it_end, func); }
	int size() const { return it_end - it_start; }
	auto operator[](int index) const { return func(it_start + index); }

  private:
	int it_start, it_end;
	Func func;
};

template <class Func> auto indexRange(int start, int end, Func func) {
	return IndexRange<Func>(start, end, func);
}

template <class Range, class Func> auto indexRange(Range range, Func func) {
	struct SubFunc {
		SubFunc(Range range, Func func) : range(move(range)), func(move(func)) {}
		auto operator()(int index) const { return func(range[index]); }
		Range range;
		Func func;
	};

	int size = end(range) - begin(range);
	return IndexRange<SubFunc>(0, size, SubFunc(move(range), move(func)));
}

template <class T> class SimpleIndexRange {
  public:
	SimpleIndexRange(int start, int end) : it_start(start), it_end(end) { DASSERT(start <= end); }

	class Iter {
	  public:
		using difference_type = int;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = T;
		using reference = T &;
		using pointer = T *;

		constexpr Iter(int index) : index(index) {}

		constexpr const Iter &operator++() {
			index++;
			return *this;
		}
		constexpr T operator*() const { return T(index); }
		constexpr int operator-(Iter it) const { return index - it.index; }

		constexpr bool operator!=(const Iter &rhs) const { return index != rhs.index; }
		constexpr bool operator==(const Iter &rhs) const { return index == rhs.index; }
		constexpr bool operator<(const Iter &rhs) const { return index < rhs.index; }

	  private:
		int index;
	};

	auto begin() const { return Iter(it_start); }
	auto end() const { return Iter(it_end); }
	int size() const { return it_end - it_start; }
	T operator[](int index) const { return T(it_start + index); }

  private:
	int it_start, it_end;
};

template <class Index> SimpleIndexRange<Index> indexRange(int begin, int end) {
	return {begin, end};
}
template <class Index> SimpleIndexRange<Index> indexRange(int count) { return {0, count}; }

template <class Index, class Range, EnableIfRange<Range>...>
SimpleIndexRange<Index> indexRange(const Range &range) {
	return {0, fwk::size(range)};
}

inline SimpleIndexRange<int> intRange(int start, int end) { return {start, end}; }
inline SimpleIndexRange<int> intRange(int size) { return {0, size}; }

template <class T, EnableIfRange<T>...> inline SimpleIndexRange<int> intRange(const T &range) {
	return {0, fwk::size(range)};
}

template <class T = int> auto wrappedPairsRange(int start, int end) {
	return IndexRange(start, end, [=](int idx) {
		int next = idx + 1;
		return pair(T(idx), T(next < end ? next : start));
	});
}

template <class T = int> auto wrappedTriplesRange(int start, int end) {
	PASSERT(end - start >= 3);
	return IndexRange(start, end, [=](int idx) {
		int next = idx + 1, next2 = idx + 2;
		return tuple(T(idx), next < end ? next : start, next2 < end ? next2 : start - end + next2);
	});
}

template <class T = int> auto wrappedPairsRange(int count) {
	return wrappedPairsRange<T>(0, count);
}
template <class T = int> auto wrappedTriplesRange(int count) {
	return wrappedTriplesRange<T>(0, count);
}

template <class T = int, class R, EnableIfRange<R>...> auto wrappedPairsRange(const R &range) {
	return wrappedPairsRange<T>(0, fwk::size(range));
}

template <class T = int, class R, EnableIfRange<R>...> auto wrappedTriplesRange(const R &range) {
	return wrappedTriplesRange<T>(0, fwk::size(range));
}
}
