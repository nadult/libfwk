// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/array.h"
#include "fwk/meta/operator.h"
#include "fwk/span.h"

#include <iterator>

namespace fwk {

struct LessCompare {
	template <class T> bool operator()(const T &a, const T &b) const { return a < b; }
};
struct GreaterCompare {
	template <class T> bool operator()(const T &a, const T &b) const { return a > b; }
};

struct IdentityFunc {
	template <class T> const T &operator()(const T &value) const { return value; }
};

// -------------------------------------------------------------------------------------------
// ---  Min/max functions  -------------------------------------------------------------------

template <class T, class U>
const Pair<T, U> &minFirst(const Pair<T, U> &lhs, const Pair<T, U> &rhs) {
	return rhs.first < lhs.first ? rhs : lhs;
}

template <class T, class U>
const Pair<T, U> &maxFirst(const Pair<T, U> &lhs, const Pair<T, U> &rhs) {
	return rhs.first > lhs.first ? rhs : lhs;
}

// TODO: move to interval ?
template <class TRange, class T = RangeBase<TRange>> Pair<T> minMax(const TRange &range) {
	if(empty(range))
		return {};
	auto it = begin(range), it_end = end(range);
	T tmin = *it;
	T tmax = tmin;

	for(++it; it != it_end; ++it) {
		tmin = min(tmin, *it);
		tmax = max(tmax, *it);
	}
	return {tmin, tmax};
}

template <class TR, class T = RangeBase<TR>> constexpr const T &max(const TR &range) {
	return *std::max_element(begin(range), end(range));
}

template <class TR, class T = RangeBase<TR>> constexpr const T &min(const TR &range) {
	return *std::min_element(begin(range), end(range));
}

template <class TR, class T = RangeBase<TR>> constexpr int maxIndex(const TR &range) {
	return std::max_element(begin(range), end(range)) - begin(range);
}

template <class TR, class T = RangeBase<TR>> constexpr int minIndex(const TR &range) {
	return std::min_element(begin(range), end(range)) - begin(range);
}

// -------------------------------------------------------------------------------------------
// ---  Testing functions  -------------------------------------------------------------------

template <c_range TRange, class Functor, class T = RangeBase<TRange>>
	requires(is_convertible<ApplyResult<Functor, T>, bool>)
bool anyOf(const TRange &range, const Functor &functor) {
	return std::any_of(begin(range), end(range), functor);
}

template <c_range TRange, class R, class T = RangeBase<TRange>>
	requires(equality_comparable<T, R>)
bool anyOf(const TRange &range, const R &ref) {
	return std::any_of(begin(range), end(range), [&](const T &val) { return val == ref; });
}

template <c_range TRange, class T = RangeBase<TRange>>
	requires(is_convertible<T, bool>)
bool anyOf(const TRange &range) {
	return std::any_of(begin(range), end(range), [&](const T &val) { return (bool)val; });
}

template <c_span TSpan, class R, class T = SpanBase<TSpan>>
	requires(equality_comparable<T, R>)
int indexOf(const TSpan &span, const R &ref) {
	for(int i = 0, span_size = fwk::size(span); i < span_size; i++)
		if(span[i] == ref)
			return i;
	return -1;
}

template <c_range TRange, class Functor, class T = RangeBase<TRange>>
	requires(is_convertible<ApplyResult<Functor, T>, bool>)
bool allOf(const TRange &range, const Functor &functor) {
	return std::all_of(begin(range), end(range), functor);
}

template <c_range TRange, class R, class T = RangeBase<TRange>>
	requires(equality_comparable<T, R>)
bool allOf(const TRange &range, const R &ref) {
	return std::all_of(begin(range), end(range), [&](const T &val) { return val == ref; });
}

template <class T, c_range R>
	requires(equality_comparable<T, RangeBase<R>>)
bool isOneOf(const T &value, const R &range) {
	return anyOf(range, value);
}

template <class T, class... Args> constexpr bool isOneOf(const T &value, const Args &...args) {
	return ((value == args) || ...);
}

template <c_range TRange> bool distinct(const TRange &range) {
	vector<RemoveConst<RangeBase<TRange>>> temp(begin(range), end(range));
	makeSortedUnique(temp);
	return fwk::size(temp) == fwk::size(range);
}

// -------------------------------------------------------------------------------------------
// ---  Sorting & set operations  ------------------------------------------------------------

template <class TSpan, class T = SpanBase<TSpan>, class Func = LessCompare>
void bubbleSort(TSpan &span, const Func &func = {}) {
	for(int i = 0, size = fwk::size(span); i < size; i++)
		for(int j = i + 1; j < size; j++)
			if(func(span[j], span[i]))
				swap(span[i], span[j]);
}

template <class T> void makeUnique(vector<T> &vec) {
	vec.erase(std::unique(begin(vec), end(vec)), end(vec));
}

template <class T> void makeSortedUnique(vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	makeUnique(vec);
}

template <class TSpan, class T = RemoveConst<SpanBase<TSpan>>>
vector<T> sortedUnique(const TSpan &span) {
	vector<T> vec(span);
	std::sort(begin(vec), end(vec));
	vec.erase(std::unique(begin(vec), end(vec)), end(vec));
	return vec;
}

template <class TSpan, class T = SpanBase<TSpan>>
	requires(!is_const<T>)
void makeSorted(TSpan &span) {
	std::sort(begin(span), end(span));
}

template <c_range TRange1, c_range TRange2> auto setDifference(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<RangeBase<TRange1>>>;
	VecType out;
	out.reserve(a.size());

	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_difference(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

template <c_range TRange1, c_range TRange2>
auto setIntersection(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<RangeBase<TRange1>>>;
	VecType out;
	out.reserve(std::min(a.size(), b.size()));

	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_intersection(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

template <c_range TRange1, c_range TRange2> auto setUnion(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<RangeBase<TRange1>>>;
	VecType out;
	out.reserve(a.size() + b.size());
	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_union(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

// -------------------------------------------------------------------------------------------
// ---  copy, fill, transform & other algorithms  --------------------------------------------

template <class T, c_range TRange> void copy(Span<T> dst, const TRange &src) {
	DASSERT(fwk::size(dst) >= fwk::size(src));
	std::copy(begin(src), end(src), begin(dst));
}

template <c_range TRange, c_span TSpan> void copy(TSpan &dst, const TRange &src) {
	copy(span(dst), src);
}

template <class T, class TRange, class T1 = RemoveConst<RangeBase<TRange>>>
	requires(is_same<T, T1>)
void copy(T *dst, const TRange &src) {
	if(size(src) > 0) {
		PASSERT(dst);
		std::copy(begin(src), end(src), dst);
	}
}

template <class T, class T1>
	requires(is_convertible<T1, T>)
void fill(Span<T> span, const T1 &value) {
	std::fill(begin(span), end(span), value);
}

template <c_range TRange, class T> void fill(TRange &range, const T &value) {
	std::fill(begin(range), end(range), value);
}

template <class TRange, class TBase = RemoveConst<RangeBase<TRange>>, class T = TBase>
auto accumulate(const TRange &range, T value = T()) {
	for(const auto &elem : range)
		value = value + elem;
	return value;
}

template <c_range TRange, class Func> auto transform(const TRange &range, const Func &func) {
	using Value = decltype(func(*range.begin()));
	static_assert(!std::is_void<Value>::value, "Func must return some value");
	vector<Value> out;
	out.reserve(range.size());
	for(const auto &elem : range)
		out.emplace_back(func(elem));
	return out;
}

template <class T, size_t size, class Func>
auto transform(const array<T, size> &input, const Func &func) {
	using Value = decltype(func(*input.begin()));
	static_assert(!std::is_void<Value>::value, "Func must return some value");
	array<Value, size> out;
	for(int n = 0; n < (int)input.size(); n++)
		out[n] = func(input[n]);
	return out;
}

template <class T, c_range TRange> vector<T> transform(const TRange &range) {
	return vector<T>(begin(range), end(range));
}

template <class T, class U, size_t size> array<T, size> transform(const array<U, size> &range) {
	array<T, size> out;
	for(size_t i = 0; i < size; i++)
		out[i] = T(range[i]);
	return out;
}

template <class TRange, class Pred = IdentityFunc, class T = RangeBase<TRange>>
int countIf(const TRange &range, const Pred &pred = {}) {
	int out = 0;
	for(const auto &elem : range)
		if(pred(elem))
			out++;
	return out;
}

template <class TRange, class Pred = IdentityFunc, class T = RangeBase<TRange>>
vector<T> filter(const TRange &range, const Pred &pred = {}, int reserve = 0) {
	vector<T> out;
	out.reserve(reserve >= 0 ? reserve : range.size());
	for(const auto &elem : range)
		if(pred(elem))
			out.emplace_back(elem);
	return out;
}

template <class T, class Pred = IdentityFunc>
void makeFiltered(vector<T> &vec, const Pred &pred = {}) {
	auto first = begin(vec), end = vec.end();
	while(first != end && pred(*first))
		++first;

	if(first == end)
		return;

	for(auto it = first; ++it != end;)
		if(pred(*it))
			*first++ = std::move(*it);

	vec.erase(first, end);
}

template <class TRange, class Base1 = RangeBase<TRange>,
		  class Base2 = RemoveConst<RangeBase<Base1>>>
vector<Base2> merge(const TRange &range_of_ranges) {
	vector<Base2> out;
	int total_size = 0;
	for(const auto &range : range_of_ranges)
		total_size += fwk::size(range);
	out.reserve(total_size);
	for(const auto &elem : range_of_ranges)
		insertBack(out, elem);
	return out;
}

template <c_range TRange> void reverse(TRange &range) { std::reverse(begin(range), end(range)); }

// -------------------------------------------------------------------------------------------
// ---  Vector modification algorithms -------------------------------------------------------

template <class T1, class Range> void insertBack(vector<T1> &into, const Range &from) {
	into.insert(end(into), begin(from), end(from));
}

template <class T1, class T2>
void insertBack(vector<T1> &into, const std::initializer_list<T2> &from) {
	into.insert(end(into), begin(from), end(from));
}

template <class T1, class Range> void insertFront(vector<T1> &into, const Range &from) {
	into.insert(begin(into), begin(from), end(from));
}

template <class T1, class T2>
void insertFront(vector<T1> &into, const std::initializer_list<T2> &from) {
	into.insert(begin(into), begin(from), end(from));
}

template <class Container, class Range> void insert(Container &into, const Range &from) {
	into.insert(begin(from), end(from));
}

// Removes all elements equal to value and returns their number
template <c_vector TVector, class T> static int removeEqual(TVector &vec, const T &value) {
	auto old_size = size(vec);
	auto it = std::remove(begin(vec), end(vec), value);
	vec.shrink(it - vec.begin());
	return old_size - size(vec);
}

template <class TVector, class Func, class TBase = VectorBase<TVector>>
void removeIf(TVector &vec, const Func &func) {
	auto it = std::remove_if(begin(vec), end(vec), func);
	vec.erase(it, vec.end());
}

template <c_vector T> void resizeUp(T &vec, int new_size) {
	if(vec.size() < new_size)
		vec.resize(new_size);
}
}
