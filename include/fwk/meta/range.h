// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/iterator.h"
#include "fwk/sys_base.h"

namespace fwk {

struct NotARange;
struct NotASpan;

namespace detail {

	template <class T, class ReqType = void> struct RangeInfo {
		template <class It, class Val> struct ValidInfo {
			using Iter = It;
			using Value = Val;
		};

		struct InvalidInfo {
			using Iter = void;
			using Value = void;
		};
		template <class U, class It1 = decltype(begin(DECLVAL(U &))),
				  class It2 = decltype(end(DECLVAL(U &))),
				  class Base1 = RemoveReference<decltype(*DECLVAL(It1 &))>,
				  class Base2 = RemoveReference<decltype(*DECLVAL(It2 &))>,
				  EnableIf<(is_same<RemoveConst<Base1>, ReqType> ||
							is_same<void, ReqType>)&&is_same<Base1, Base2>>...>
		static auto test(U &) -> ValidInfo<It1, Base1>;
		template <class U> static InvalidInfo test(...);

		using Info = decltype(test<T>(DECLVAL(T &)));
		static constexpr bool value = !is_same<Info, InvalidInfo>;
		using MaybeInfo = If<value, Info, NotARange>;
	};

	// TODO: don't use Req parameter here, it causes unnecessary instantiations
	template <class T, class ReqType = void> struct SpanInfo {
		template <class Val, bool has_data_> struct ValidInfo {
			enum { is_const = fwk::is_const<Val>, has_data = has_data_ };
			using Value = Val;
		};

		struct InvalidInfo {
			enum { is_const = false, has_data = false };
			using Value = void;
		};

		template <class U, class It1 = decltype(begin(DECLVAL(U &))),
				  class It2 = decltype(end(DECLVAL(U &))),
				  EnableIf<is_same<It1, It2> && std::is_pointer<It1>::value>...,
				  class Base = RemovePointer<It1>,
				  EnableIf<(is_same<Base, ReqType> || is_same<void, ReqType>)>...>
		static auto test(PriorityTag1, U &) -> ValidInfo<Base, false>;

		template <class U, class Base = RemovePointer<decltype(DECLVAL(U &).data())>,
				  EnableIf<is_same<RemoveConst<Base>, ReqType> || is_same<void, ReqType>>...,
				  EnableIf<is_convertible<decltype(DECLVAL(U &).size()), int>, int>...>
		static auto test(PriorityTag0, U &) -> ValidInfo<Base, true>;

		template <class U> static InvalidInfo test(...);

		using Info = decltype(test<T>(PriorityTag1(), DECLVAL(T &)));
		static constexpr bool value = !is_same<Info, InvalidInfo>;
		using MaybeInfo = If<value, Info, NotASpan>;
	};
}

template <class T> constexpr bool is_range = detail::RangeInfo<T>::value;
template <class T> constexpr bool is_span = detail::SpanInfo<T>::value;

template <class T, class Req = void>
using EnableIfRange = EnableIf<detail::RangeInfo<T, Req>::value, NotARange>;
template <class T, class Req = void>
using EnableIfSpan = EnableIf<detail::SpanInfo<T, Req>::value, NotARange>;

template <class T> using RangeBase = typename detail::RangeInfo<T>::MaybeInfo::Value;
template <class T> using RangeIter = typename detail::RangeInfo<T>::MaybeInfo::Iter;
template <class T> using SpanBase = typename detail::SpanInfo<T>::MaybeInfo::Value;

template <class TSpan, class Base = SpanBase<TSpan>> Base *data(TSpan &range) {
	if constexpr(detail::SpanInfo<TSpan>::Info::has_data)
		return range.data();
	else
		return begin(range);
}

template <class TSpan, class Base = SpanBase<TSpan>> const Base *data(const TSpan &range) {
	if constexpr(detail::SpanInfo<TSpan>::Info::has_data)
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
