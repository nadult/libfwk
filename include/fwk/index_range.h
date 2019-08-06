// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/range.h"

namespace fwk {

// Examples:
//
// intRange(4):            0, 1, 2, 3
// intRange(10, 15):       10, 11, 12, 13, 14
// indexRange<T>(4):       T(0), T(1), T(2), T(3)
// pairsRange(4):          (0, 1), (1, 2), (2, 3)
// wrappedPairsRange(4):   (0, 1), (1, 2), (2, 3), (3, 0)
// wrappedTriplesRange(3): (0, 1, 2), (1, 2, 0), (2, 0, 1)
//
// ??Range( some_range ) -> ??Range(0, size(some_range))

// IndexRange has to exist as long as any of the iterators belonging to it
template <class Transform = None, class Filter = None> class IndexRange {
  public:
	constexpr IndexRange(int start, int end, Transform trans = Transform(),
						 Filter filter = Filter())
		: m_start(start), m_end(end), m_trans(move(trans)), m_filter(move(filter)) {
		PASSERT(start <= end);
		if constexpr(!is_same<Filter, None>) {
			while(m_start != m_end && !m_filter(m_start))
				m_start++;
		}
	}

	static auto apply(const Transform &trans, int index) {
		if constexpr(is_same<Transform, None>)
			return index;
		else
			return trans(index);
	}

	using Value = decltype(apply(DECLVAL(Transform), 0));

	struct Iter {
		using difference_type = int;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = Value;
		using reference = value_type &;
		using pointer = value_type *;

		constexpr Iter(int index, const IndexRange &base) : index(index), base(base) {}

		constexpr const Iter &operator++() {
			index++;
			if constexpr(!is_same<Filter, None>) {
				while(index != base.m_end && !base.m_filter(index))
					index++;
			}
			return *this;
		}
		Value operator*() const { return apply(base.m_trans, index); }

		constexpr int operator-(const Iter &rhs) const {
			if constexpr(is_same<Filter, None>)
				return index - rhs.index;
			else {
				int idx = rhs.index, count = 0;
				while(idx < index) {
					if(base.m_filter(idx))
						count++;
					idx++;
				}
				return count;
			}
		}

		constexpr bool operator!=(const Iter &rhs) const { return index != rhs.index; }
		constexpr bool operator==(const Iter &rhs) const { return index == rhs.index; }
		constexpr bool operator<(const Iter &rhs) const { return index < rhs.index; }

		const IndexRange &base;
		int index;
	};

	auto operator[](int index) const { return apply(m_trans, m_start + index); }

	auto begin() const { return Iter(m_start, *this); }
	auto end() const { return Iter(m_end, *this); }
	int size() const { return end() - begin(); }

  private:
	int m_start, m_end;
	Transform m_trans;
	Filter m_filter;
};

template <class Transform, class Filter = None>
auto indexRange(int start, int end, const Transform &trans, const Filter &filter = none) {
	return IndexRange<Transform, Filter>(start, end, trans, filter);
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
	SimpleIndexRange(int start, int end) : it_start(start), it_end(end) { PASSERT(start <= end); }

	class Iter {
	  public:
		using difference_type = int;
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = T;
		using reference = value_type &;
		using pointer = value_type *;

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

inline SimpleIndexRange<int> intRange(int start, int end) { return {start, end}; }
inline SimpleIndexRange<int> intRange(int size) { return {0, size}; }
template <class T, EnableIfRange<T>...> inline SimpleIndexRange<int> intRange(const T &range) {
	return {0, fwk::size(range)};
}

template <class T = int> auto pairsRange(int start, int end) {
	return IndexRange(start, end - 1, [=](int idx) { return pair(T(idx), T(idx + 1)); });
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

#define RANGE_HELPERS(name)                                                                        \
	template <class T = int> auto name(int count) { return name<T>(0, count); }                    \
	template <class T = int, class R, EnableIfRange<R>...> auto name(const R &range) {             \
		return name<T>(0, fwk::size(range));                                                       \
	}

RANGE_HELPERS(wrappedPairsRange)
RANGE_HELPERS(wrappedTriplesRange)
RANGE_HELPERS(pairsRange)
RANGE_HELPERS(indexRange)

#undef RANGE_HELPERS
}
