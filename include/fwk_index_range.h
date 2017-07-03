// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_INDEX_RANGE_H
#define FWK_INDEX_RANGE_H

#include "fwk_range.h"

namespace fwk {

// TODO: better name to differentiate from Range<> and CSpan<>
// Or maybe simply rename Range<> to something else ?
//
// IndexRange has to exist as long as any of the iterators belonging to it
template <class Func> class IndexRange {
  public:
	IndexRange(int start, int end, Func func) : it_start(start), it_end(end), func(move(func)) {
		DASSERT(start <= end);
	}

	class Iter {
	  public:
		using difference_type = int;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = decltype((*((Func *)nullptr))(0));
		using reference = value_type &;
		using pointer = value_type *;

		constexpr Iter(int index, const Func &func) : index(index), func(func) {}

		constexpr const Iter &operator++() {
			index++;
			return *this;
		}
		auto operator*() const { return func(index); }
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

template <class Index> SimpleIndexRange<Index> indexRange(int min, int max) { return {min, max}; }

template <class Index> SimpleIndexRange<Index> indexRange(int count) { return {0, count}; }

template <class Index, class Range, EnableIfRange<Range>...>
SimpleIndexRange<Index> indexRange(const Range &range) {
	return {0, range.size()};
}

inline SimpleIndexRange<int> intRange(int min, int max) { return {min, max}; }
inline SimpleIndexRange<int> intRange(int size) { return {0, size}; }

template <class T, EnableIfRange<T>...> inline SimpleIndexRange<int> intRange(const T &range) {
	return {0, range.size()};
}
}

#endif
