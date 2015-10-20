/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_RANGE_H
#define FWK_RANGE_H

#include "fwk_base.h"

namespace fwk {

template <class T> class RangeIterator : public std::iterator<std::random_access_iterator_tag, T> {
  public:
	constexpr RangeIterator(T *pointer) noexcept : m_pointer(pointer) {}
	constexpr auto operator+(int offset) const noexcept {
		return RangeIterator(m_pointer + offset);
	}
	constexpr auto operator-(int offset) const noexcept {
		return RangeIterator(m_pointer - offset);
	}

	constexpr int operator-(const RangeIterator &rhs) const noexcept {
		return m_pointer - rhs.m_pointer;
	}
	constexpr T &operator*() const noexcept { return *m_pointer; }
	constexpr T &operator->() const noexcept { return *m_pointer; }
	constexpr bool operator==(const RangeIterator &rhs) const noexcept {
		return m_pointer == rhs.m_pointer;
	}
	constexpr bool operator<(const RangeIterator &rhs) const noexcept {
		return m_pointer < rhs.m_pointer;
	}
	RangeIterator &operator++() noexcept {
		m_pointer++;
		return *this;
	}

  private:
	T *m_pointer;
};

template <class T> using CRangeIterator = RangeIterator<const T>;

template <class T> class PodArray;

// Simple wrapper class for ranges of values
// The user is responsible for making sure, that the values
// exist while Range pointing to them is in use
// TODO: add possiblity to reinterpret Range of uchars into range of chars, etc.
template <class T, int min_size = 0> class Range {
  public:
	using value_type = typename std::remove_const<T>::type;
	enum { is_const = std::is_const<T>::value, minimum_size = min_size };
	using vector_type =
		typename std::conditional<is_const, const vector<value_type>, vector<value_type>>::type;
	using pod_array_type =
		typename std::conditional<is_const, const PodArray<value_type>, PodArray<value_type>>::type;

	// friend class CRange<typename std::remove_const<T>::type>;
	Range(T *data, int size) : m_data(data), m_size(size) {
		DASSERT(m_data || m_size == 0);
		DASSERT(m_size >= min_size);
	}
	Range(T *begin, T *end) : Range(begin, (int)(end - begin)) {}
	Range(vector_type &vec) : Range(vec.data(), vec.size()) {}
	Range(pod_array_type &array) : Range(array.data(), array.size()) {}
	template <int N> Range(value_type(&array)[N]) : m_data(array), m_size(N) {
		static_assert(N >= min_size, "Array too small");
	}
	template <int N> Range(const value_type(&array)[N]) : m_data(array), m_size(N) {
		static_assert(N >= min_size, "Array too small");
	}
	template <size_t N> Range(array<value_type, N> &arr) : m_data(arr.data()), m_size(N) {
		static_assert(N >= min_size, "Array too small");
	}
	template <size_t N> Range(const array<value_type, N> &arr) : m_data(arr.data()), m_size(N) {
		static_assert(N >= min_size, "Array too small");
	}
	Range() : m_data(nullptr), m_size(0) {
		static_assert(min_size == 0, "Cannot construct empty range with minimum size > 0");
	}
	template <int other_size>
	Range(Range<T, other_size> range)
		: m_data(range.m_data), m_size(range.m_size) {
		static_assert(other_size >= min_size, "Range too small");
	}
	template <class U = T>
	Range(const Range<value_type, min_size> &range,
		  typename std::enable_if<std::is_const<U>::value>::type * = nullptr)
		: m_data(range.m_data), m_size(range.m_size) {}
	template <class U = T>
	Range(const std::initializer_list<T> &list,
		  typename std::enable_if<std::is_const<U>::value>::type * = nullptr)
		: Range(list.begin(), list.end()) {}
	operator vector<value_type>() const { return vector<value_type>(cbegin(), cend()); }

	auto cbegin() const noexcept { return CRangeIterator<T>(m_data); }
	auto cend() const noexcept { return CRangeIterator<T>(m_data + m_size); }
	auto begin() const noexcept { return RangeIterator<T>(m_data); }
	auto end() const noexcept { return RangeIterator<T>(m_data + m_size); }

	T *data() const noexcept { return m_data; }
	int size() const noexcept { return m_size; }
	bool empty() const noexcept { return m_size == 0; }

	T &operator[](int idx) const {
		DASSERT(idx >= 0 && idx < m_size);
		return m_data[idx];
	}

  private:
	T *const m_data;
	const int m_size;
};

template <class T, int min_size = 0> using CRange = Range<const T, min_size>;

template <class T> struct ConvertsToRange {
	template <class T1,
			  typename Base = typename std::remove_pointer<decltype(((T1 *)nullptr)->data())>::type,
			  typename TRange = decltype(Range<Base>(*(T1 *) nullptr))>
	constexpr static bool converts(T1 *) {
		return true;
	}
	constexpr static bool converts(...) { return false; }
	enum { value = converts((T *)nullptr) };
};

template <class Container> auto makeRange(Container &container) {
	using type = typename std::remove_pointer<decltype(container.data())>::type;
	return Range<type>(container.data(), container.size());
}

template <class Container> auto makeRange(const Container &container) {
	using type = typename std::remove_pointer<decltype(container.data())>::type;
	return CRange<type>(container.data(), container.size());
}

template <class T> auto makeRange(const std::initializer_list<T> &list) {
	return CRange<T>(list.begin(), list.end());
}

template <class Target, class T> auto reinterpretRange(Range<T> range) {
	using out_type = typename std::conditional<std::is_const<T>::value, const Target, Target>::type;
	return Range<out_type>(reinterpret_cast<out_type *>(range.data()),
						   size_t(range.size()) * sizeof(T) / sizeof(Target));
}

// TODO: write more of these
template <class Range, class Functor> bool anyOf(const Range &range, Functor functor) {
	return std::any_of(begin(range), end(range), functor);
}

template <class Range, class Functor> bool allOf(const Range &range, Functor functor) {
	return std::all_of(begin(range), end(range), functor);
}

template <class T1, class Container> void insertBack(vector<T1> &into, const Container &from) {
	into.insert(end(into), begin(from), end(from));
}

template <class T> void makeUnique(vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	vec.resize(std::unique(begin(vec), end(vec)) - vec.begin());
}

template <class T> void makeSorted(vector<T> &vec) { std::sort(begin(vec), end(vec)); }

template <class Range1, class Range2> auto setDifference(const Range1 &a, const Range2 &b) {
	using VecType = vector<typename Range1::value_type>;
	VecType out(a.size());
	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	auto it = std::set_difference(begin(va), end(va), begin(vb), end(vb), begin(out));
	out.resize(it - begin(out));
	return out;
}

template <class Range1, class Range2> auto setIntersection(const Range1 &a, const Range2 &b) {
	using VecType = vector<typename Range1::value_type>;
	VecType out(std::min(a.size(), b.size()));
	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	auto it = std::set_intersection(begin(va), end(va), begin(vb), end(vb), begin(out));
	out.resize(it - begin(out));
	return out;
}

template <class Range1, class Range2> auto setUnion(const Range1 &a, const Range2 &b) {
	using VecType = vector<typename Range1::value_type>;
	VecType out(a.size() + b.size());
	VecType va(begin(a), end(a)), vb(begin(b), end(b));
	makeSorted(va);
	makeSorted(vb);
	auto it = std::set_union(begin(va), end(va), begin(vb), end(vb), begin(out));
	out.resize(it - begin(out));
	return out;
}

template <class Range, class Func> auto findMin(const Range &range, const Func &func) {
	using Value = decltype(func(range[0]));

	if(range.empty())
		return make_pair(-1, Value());

	int best_index = 0;
	Value best_val = func(range[0]);

	for(int n = 1; n < (int)range.size(); n++) {
		auto val = func(range[n]);
		if(val < best_val) {
			best_val = val;
			best_index = n;
		}
	}

	return make_pair(best_index, best_val);
}

template <class Range, class Func> auto findMax(const Range &range, const Func &func) {
	using Value = decltype(func(range[0]));

	if(range.empty())
		return make_pair(-1, Value());

	int best_index = 0;
	Value best_val = func(range[0]);

	for(int n = 1; n < (int)range.size(); n++) {
		auto val = func(range[n]);
		if(val > best_val) {
			best_val = val;
			best_index = n;
		}
	}

	return make_pair(best_index, best_val);
}

template <class T1, class T2>
void insertBack(vector<T1> &into, const std::initializer_list<T2> &from) {
	into.insert(end(into), begin(from), end(from));
}

template <class T1, class OutputIterator> void copy(Range<T1> range, OutputIterator iter) {
	return std::copy(begin(range), end(range), iter);
}

template <class Range, class Func> auto transform(const Range &range, const Func &func) {
	using Value = decltype(func(*range.begin()));
	static_assert(!std::is_void<Value>::value, "Func must return some value");
	vector<Value> out;
	out.reserve(range.size());
	for(auto elem : range)
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
}

#endif
