// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/iterator.h"
#include "fwk/sys_base.h"

namespace fwk {

struct NotARange;
struct NotASpan;
struct NotAVector;

namespace detail {
	template <class T> struct RangeInfo {
		template <class It, class Val> struct Info {
			using Iter = It;
			using Value = Val;
		};

		FWK_SFINAE_TYPE(BeginItType, T, begin(DECLVAL(U &)))
		FWK_SFINAE_TYPE(EndItType, T, end(DECLVAL(U &)))
		FWK_SFINAE_TYPE(BaseType_, BeginItType, *DECLVAL(U &))
		using BaseType = RemoveReference<BaseType_>;

		static constexpr bool value =
			!is_same<BaseType, InvalidType> && equality_comparable<BeginItType, EndItType>;
		using MaybeInfo = If<value, Info<BeginItType, BaseType>, NotARange>;
	};

	template <class T> struct SpanInfo {
		template <class Val> struct Info { using Value = Val; };

		FWK_SFINAE_TYPE(DataType, T, DECLVAL(U &).data())
		FWK_SFINAE_TYPE(SizeType, T, DECLVAL(U &).size())

		using RInfo = RangeInfo<T>;
		static constexpr bool data_mode =
			std::is_pointer<DataType>::value && is_convertible<SizeType, int>;
		static constexpr bool value =
			data_mode || (RInfo::value && std::is_pointer<typename RInfo::BeginItType>::value);
		using Base = If<data_mode, RemovePointer<DataType>, typename RInfo::BaseType>;

		using MaybeInfo = If<value, Info<Base>, NotASpan>;
	};

	template <class T> constexpr bool is_vector = false;
	template <class T> constexpr bool is_vector<Vector<T>> = true;
	template <class T> constexpr bool is_vector<PodVector<T>> = true;
	template <class T, int size> constexpr bool is_vector<SmallVector<T, size>> = true;
	template <class T, int size> constexpr bool is_vector<StaticVector<T, size>> = true;

	template <class T> struct VectorInfo {
		static constexpr bool value = is_vector<T>;
		using MaybeInfo = If<value, typename RangeInfo<T>::MaybeInfo, NotAVector>;
	};
}

// Conditions for range:
// These expressions must be valid: begin(range), end(range)
// Iterators returned by begin(range) and end(range) must be comparable
// Iterator returned by begin(range) must be dereferencible
template <class T> constexpr bool is_range = detail::RangeInfo<T>::value;

// Conditions for span:
// It either must be a range where iterators are simple pointers
// Or it must provide data() (which returns a pointer) and size()
template <class T> constexpr bool is_span = detail::SpanInfo<T>::value;

// Conditions for vector are not clearly defined yet (TODO)
// This test is useful for some generic algorithms
template <class T> constexpr bool is_vector = detail::is_vector<T>;

// TODO: add concepts: sparse_span, sparse_vector ? A lot of containers could satisfy
// sparse_span concept

template <class T> using EnableIfRange = EnableIf<detail::RangeInfo<T>::value, NotARange>;
template <class T> using EnableIfSpan = EnableIf<detail::SpanInfo<T>::value, NotASpan>;

template <class T> using RangeBase = typename detail::RangeInfo<T>::MaybeInfo::Value;
template <class T> using RangeIter = typename detail::RangeInfo<T>::MaybeInfo::Iter;
template <class T> using SpanBase = typename detail::SpanInfo<T>::MaybeInfo::Value;
template <class T> using VectorBase = typename detail::VectorInfo<T>::MaybeInfo::Value;

template <class TSpan, class Base = SpanBase<TSpan>> Base *data(TSpan &range) {
	if constexpr(detail::SpanInfo<TSpan>::data_mode)
		return range.data();
	else
		return begin(range);
}

template <class TSpan, class Base = SpanBase<TSpan>> const Base *data(const TSpan &range) {
	if constexpr(detail::SpanInfo<TSpan>::data_mode)
		return range.data();
	else
		return begin(range);
}

template <class TRange, EnableIfRange<TRange>...> bool empty(const TRange &range) {
	return begin(range) == end(range);
}

template <class TRange, EnableIfRange<TRange>...> int size(const TRange &range) {
	return fwk::distance(begin(range), end(range));
}

template <class TRange, EnableIfRange<TRange>..., class Ret = decltype(*begin(DECLVAL(TRange)))>
Ret front(TRange &&range) {
	DASSERT(!empty(range));
	return *begin(range);
}

template <class TRange, EnableIfRange<TRange>..., class Ret = decltype(*begin(DECLVAL(TRange)))>
Ret back(TRange &&range) {
	DASSERT(!empty(range));
	auto it = end(range);
	return *--it;
}
}
