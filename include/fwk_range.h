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

	// Idea from: Range V3
	template <int I> struct PriorityTag : PriorityTag<I - 1> {};
	template <> struct PriorityTag<0> {};

	template <class T, class ReqType = void> struct RangeInfo {
		template <class It, class Val> struct ValidInfo {
			using Iter = It;
			using Value = Val;
		};

		struct InvalidInfo {
			using Iter = void;
			using Value = void;
		};
		template <class U, class It1 = decltype(begin(declval<U &>())),
				  class It2 = decltype(end(declval<U &>())),
				  class Base1 = typename std::remove_reference<decltype(*declval<It1 &>())>::type,
				  class Base2 = typename std::remove_reference<decltype(*declval<It2 &>())>::type,
				  EnableIf<(isSame<RemoveConst<Base1>, ReqType>() || isSame<void, ReqType>()) &&
						   isSame<Base1, Base2>()>...>
		static auto test(U &) -> ValidInfo<It1, Base1>;
		template <class U> static InvalidInfo test(...);

		using Info = decltype(test<T>(declval<T &>()));
		enum { value = !isSame<Info, InvalidInfo>() };
		using MaybeInfo = Conditional<value, Info, NotARange>;
	};

	// TODO: don't use Req parameter here, it causes unnecessary instantiations
	template <class T, class ReqType = void> struct SpanInfo {
		template <class Val, bool has_data_> struct ValidInfo {
			enum { is_const = std::is_const<Val>::value, has_data = has_data_ };
			using Value = Val;
		};

		struct InvalidInfo {
			enum { is_const = false, has_data = false };
			using Value = void;
		};

		template <class U, class It1 = decltype(begin(declval<U &>())),
				  class It2 = decltype(end(declval<U &>())),
				  EnableIf<isSame<It1, It2>() && std::is_pointer<It1>::value>...,
				  class Base = typename std::remove_pointer<It1>::type,
				  EnableIf<(isSame<Base, ReqType>() || isSame<void, ReqType>())>...>
		static auto test(PriorityTag<1>, U &) -> ValidInfo<Base, false>;

		template <
			class U,
			class Base = typename std::remove_pointer<decltype(declval<U &>().data())>::type,
			EnableIf<isSame<RemoveConst<Base>, ReqType>() || isSame<void, ReqType>()>...,
			EnableIf<std::is_convertible<decltype(declval<U &>().size()), int>::value, int>...>
		static auto test(PriorityTag<0>, U &) -> ValidInfo<Base, true>;

		template <class U> static InvalidInfo test(...);

		using Info = decltype(test<T>(PriorityTag<1>(), declval<T &>()));
		enum { value = !isSame<Info, InvalidInfo>() };
		using MaybeInfo = Conditional<value, Info, NotASpan>;
	};
}

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

template <class T> constexpr bool isRange() { return detail::RangeInfo<T>::value; }
template <class T> constexpr bool isSpan() { return detail::SpanInfo<T>::value; }

template <class TRange, EnableIfRange<TRange>...> bool empty(const TRange &range) {
	return begin(range) == end(range);
}

template <class TRange, EnableIfRange<TRange>...> int size(const TRange &range) {
	return std::distance(begin(range), end(range));
}

template <class TRange, EnableIfRange<TRange>..., class Ret = decltype(*begin(declval<TRange>()))>
Ret front(TRange &&range) {
	DASSERT(!empty(range));
	return *begin(range);
}

template <class TRange, EnableIfRange<TRange>..., class Ret = decltype(*begin(declval<TRange>()))>
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

// TODO: write more of these
template <class Range, class Functor> bool anyOf(const Range &range, Functor functor) {
	return std::any_of(begin(range), end(range), functor);
}

template <class Range, class Functor> bool allOf(const Range &range, Functor functor) {
	return std::all_of(begin(range), end(range), functor);
}

template <class T, class TRange, EnableIfRange<TRange, T>...>
inline bool isOneOf(const T &value, const TRange &range) {
	return anyOf(range, [&](const auto &v) { return value == v; });
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
	enum { is_const = isConst<T>(), minimum_size = min_size };
	using vector_type = Conditional<is_const, const vector<value_type>, vector<value_type>>;
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

	Span(T *begin, T *end) : Span(begin, (int)(end - begin), no_asserts_tag) {
		DASSERT(end >= begin);
	}
	Span(vector_type &vec) : Span(vec.data(), vec.size(), no_asserts_tag) {}
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

	template <class U = T, EnableIf<isConst<U>()>...>
	Span(const Span<value_type, min_size> &range) : m_data(range.data()), m_size(range.size()) {}

	Span(const std::initializer_list<T> &list) : Span(list.begin(), list.end()) {}
	operator vector<value_type>() const { return vector<value_type>(begin(), end()); }

	template <class TSpan, class Base = SpanBase<TSpan>,
			  EnableIf<isSame<Base, T>() && !isSame<TSpan, Span>()>...>
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

	Span subSpan(int start, int end) const {
		DASSERT(start >= 0 && start < end);
		DASSERT(end <= m_size);
		return {m_data + start, end - start};
	}

	template <class U, EnableIf<isSame<RemoveConst<U>, value_type>()>...>
	bool operator==(Span<U> rhs) const {
		if(m_size != rhs.size())
			return false;
		for(int n = 0; n < m_size; n++)
			if(m_data[n] != rhs[n])
				return false;
		return true;
	}

	template <class U, EnableIf<isSame<RemoveConst<U>, value_type>()>...>
	bool operator<(Span<U> rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

  private:
	T *m_data;
	int m_size;
};

constexpr inline bool compatibleSizes(size_t a, size_t b) {
	return a > b ? a % b == 0 : b % a == 0;
}

template <class U, class T, EnableIf<compatibleSizes(sizeof(T), sizeof(U))>...>
auto reinterpret(Span<T> span) {
	using out_type = Conditional<isConst<T>(), const U, U>;
	auto new_size = size_t(span.size()) * sizeof(T) / sizeof(U);
	return Span<out_type>(reinterpret_cast<out_type *>(span.data()), span.size());
}

template <class T, int min_size = 0> using CSpan = Span<const T, min_size>;

template <class TSpan, class T = SpanBase<TSpan>> Span<T> makeSpan(TSpan &span) {
	return Span<T>(span);
}

template <class TSpan, class T = SpanBase<TSpan>> CSpan<T> makeCSpan(const TSpan &span) {
	return CSpan<T>(span);
}

template <class T> void makeUnique(vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	vec.erase(std::unique(begin(vec), end(vec)), end(vec));
}

template <class TSpan, class T = SpanBase<TSpan>, EnableIf<!isConst<T>()>...>
void makeSorted(TSpan &span) {
	std::sort(begin(span), end(span));
}

template <class TRange1, class TRange2, class T1 = RangeBase<TRange1>,
		  class T2 = RangeBase<TRange2>>
auto setDifference(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<T1>>;
	VecType out;
	out.reserve(a.size());

	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_difference(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

template <class TRange1, class TRange2, class T1 = RangeBase<TRange1>,
		  class T2 = RangeBase<TRange2>>
auto setIntersection(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<T1>>;
	VecType out;
	out.reserve(std::min(a.size(), b.size()));

	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_intersection(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

template <class TRange1, class TRange2, class T1 = RangeBase<TRange1>,
		  class T2 = RangeBase<TRange2>>
auto setUnion(const TRange1 &a, const TRange2 &b) {
	using VecType = vector<RemoveConst<T1>>;
	VecType out;
	out.reserve(a.size() + b.size());
	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	std::set_union(begin(va), end(va), begin(vb), end(vb), std::back_inserter(out));
	return out;
}

// TODO: move to interval ?
template <class TRange, class T = RangeBase<TRange>> pair<T, T> minMax(const TRange &range) {
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

template <class TRange, EnableIfRange<TRange>..., class TSpan, EnableIfSpan<TSpan>...>
void copy(TSpan &dst, const TRange &src) {
	DASSERT(fwk::size(dst) >= fwk::size(src));
	std::copy(begin(src), end(src), begin(dst));
}

template <class TRange, EnableIfRange<TRange>..., class T>
void fill(TRange &range, const T &value) {
	std::fill(begin(range), end(range), value);
}

template <class TRange, class Func, EnableIfRange<TRange>...>
auto transform(const TRange &range, const Func &func) {
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

template <class T, class TRange, EnableIfRange<TRange>...>
vector<T> transform(const TRange &range) {
	return vector<T>(begin(range), end(range));
}

template <class T, class U, size_t size> array<T, size> transform(const array<U, size> &range) {
	array<T, size> out;
	for(size_t i = 0; i < size; i++)
		out[i] = T(range[i]);
	return out;
}

template <class TRange, class Pred, class T = RangeBase<TRange>>
int countIf(const TRange &range, const Pred &pred) {
	int out = 0;
	for(const auto &elem : range)
		if(pred(elem))
			out++;
	return out;
}

template <class TRange, class Filter, class T = RangeBase<TRange>>
vector<T> filter(const TRange &range, const Filter &filter, int reserve = 0) {
	vector<T> out;
	out.reserve(reserve >= 0 ? reserve : range.size());
	for(const auto &elem : range)
		if(filter(elem))
			out.emplace_back(elem);
	return out;
}

template <class T, class Filter> void makeFiltered(vector<T> &vec, const Filter &filter) {
	auto first = begin(vec), end = vec.end();
	while(first != end && filter(*first))
		++first;

	if(first == end)
		return;

	for(auto it = first; ++it != end;)
		if(filter(*it))
			*first++ = move(*it);

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

template <class TRange, EnableIfRange<TRange>...> bool distinct(const TRange &range) {
	vector<RemoveConst<RangeBase<TRange>>> temp(begin(range), end(range));
	makeUnique(temp);
	return fwk::size(temp) == fwk::size(range);
}
}

#endif
