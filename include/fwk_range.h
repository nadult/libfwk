// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_RANGE_H
#define FWK_RANGE_H

#include "fwk/sys_base.h"
#include "fwk_vector.h"

namespace fwk {

// TODO: rename to fwk_ranges; move out fwk_span ?
// TODO: fwk_concepts ?

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

template <class TRange, class TBase = RemoveConst<RangeBase<TRange>>, class T = TBase>
auto accumulate(const TRange &range, T value = T()) {
	for(const auto &elem : range)
		value = value + elem;
	return value;
}

template <class TRange, class Functor, class T = RangeBase<TRange>,
		  EnableIf<is_convertible<ApplyResult<Functor, T>, bool>>...>
bool anyOf(const TRange &range, const Functor &functor) {
	return std::any_of(begin(range), end(range), functor);
}

template <class TRange, class R, class T = RangeBase<TRange>,
		  EnableIf<is_same<EqualResult<T, R>, bool>>...>
bool anyOf(const TRange &range, const R &ref) {
	return std::any_of(begin(range), end(range), [&](const T &val) { return val == ref; });
}

template <class TRange, class Functor, class T = RangeBase<TRange>,
		  EnableIf<is_convertible<ApplyResult<Functor, T>, bool>>...>
bool allOf(const TRange &range, const Functor &functor) {
	return std::all_of(begin(range), end(range), functor);
}

template <class TRange, class R, class T = RangeBase<TRange>,
		  EnableIf<is_same<EqualResult<T, R>, bool>>...>
bool allOf(const TRange &range, const R &ref) {
	return std::all_of(begin(range), end(range), [&](const T &val) { return val == ref; });
}

template <class T, class TRange, EnableIfRange<TRange, T>...>
bool isOneOf(const T &value, const TRange &range) {
	return anyOf(range, value);
}

template <class TRange, class T = RangeBase<TRange>> constexpr const T &max(const TRange &range) {
	return *std::max_element(begin(range), end(range));
}

template <class TRange, class T = RangeBase<TRange>> constexpr const T &min(const TRange &range) {
	return *std::min_element(begin(range), end(range));
}

// TODO: add isComparable
template <class T> constexpr bool isOneOf(const T &value) { return false; }

template <class T, class Arg1, class... Args>
constexpr bool isOneOf(const T &value, const Arg1 &arg1, const Args &... args) {
	return value == arg1 || isOneOf(value, args...);
}

// Span is a range of elements spanning continuous range of memory (like vector, array, etc.).
// Span is very light-weight: it keeps a pointer and a size; User is responsible for making sure
//   that the container with values exist as long as Span exists.
template <class T, int min_size = 0> class Span {
  public:
	using value_type = RemoveConst<T>;
	enum { is_const = fwk::is_const<T>, minimum_size = min_size };
	using vector_type = If<is_const, const vector<value_type>, vector<value_type>>;
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

	Span(T *begin, T *end) : Span(begin, (int)(end - begin), no_asserts) { DASSERT(end >= begin); }
	Span(vector_type &vec) : Span(vec.data(), vec.size(), no_asserts) {}
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

	template <int rhs_min_size, EnableIf<rhs_min_size >= min_size>...>
	Span(Span<T, rhs_min_size> range) : m_data(range.data()), m_size(range.size()) {}

	template <class U = T, EnableIf<fwk::is_const<U>>...>
	Span(const Span<value_type, min_size> &range) : m_data(range.data()), m_size(range.size()) {}

	Span(const std::initializer_list<T> &list) : Span(list.begin(), list.end()) {}
	operator vector<value_type>() const { return vector<value_type>(begin(), end()); }

	template <class TSpan, class Base = SpanBase<TSpan>,
			  EnableIf<is_same<Base, T> && !is_same<TSpan, Span>>...>
	Span(TSpan &rhs) : Span(fwk::data(rhs), fwk::size(rhs)) {}

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
		PASSERT(inRange(idx));
		return m_data[idx];
	}

	Span subSpan(int start) const {
		DASSERT(start >= 0 && start < m_size);
		return {m_data + start, m_size - start};
	}

	Span subSpan(int start, int end) const {
		DASSERT(start >= 0 && start < end);
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

}

#endif
