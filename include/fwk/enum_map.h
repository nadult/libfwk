// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"
#include "fwk_range.h"
#include <array>

namespace fwk {

template <class Enum, class T> class EnumMap {
  public:
	static constexpr int size_ = count<Enum>();

	static_assert(is_enum<Enum>,
				  "EnumMap<> can only be used for enums specified with DEFINE_ENUM*");

	static void checkPairs(CSpan<Pair<Enum, T>> pairs) {
		unsigned long long flags = 0;
		static_assert(size_ <= 64);

		for(auto &pair : pairs) {
			auto bit = 1ull << int(pair.first);
			if(flags & bit)
				FWK_FATAL("Enum entry duplicated: %s", toString(pair.first));
			flags |= bit;
		}

		if(pairs.size() != size_)
			FWK_FATAL("Invalid number of entries specified: %d (expected: %d)", pairs.size(),
					  size_);
	}

	// ---------------------------------------------------------------
	// ----- Initializers with all values specified ------------------

	template <class U, int N, EnableIf<is_convertible<U, T>>...> EnumMap(CSpan<U, N> values) {
		DASSERT(values.size() == size_ && "Invalid number of values specified");
		std::copy(values.begin(), values.end(), m_data.begin());
	}
	template <class U, int N, EnableIf<is_convertible<U, T>>...>
	EnumMap(CSpan<Pair<Enum, U>, N> pairs) {
		IF_DEBUG(checkPairs(pairs));
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	template <class USpan, class U = SpanBase<USpan>,
			  EnableIf<is_convertible<U, T> || is_convertible<U, Pair<Enum, T>>>...>
	EnumMap(const USpan &span) : EnumMap(cspan(span)) {}

	template <int N> EnumMap(const Pair<Enum, T> (&arr)[N]) : EnumMap(CSpan<Pair<Enum, T>>(arr)) {
		static_assert(N == size_, "Invalid number of enum-value pairs specified");
	}

	template <int N> EnumMap(const T (&arr)[N]) : EnumMap(CSpan<T>(arr)) {
		static_assert(N == size_, "Invalid number of values specified");
	}

	// ---------------------------------------------------------------
	// ----- Initializers with default value -------------------------

	EnumMap(CSpan<Pair<Enum, T>> pairs, T default_value) {
		m_data.fill(default_value);
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(std::initializer_list<Pair<Enum, T>> list, T default_value)
		: EnumMap(CSpan<Pair<Enum, T>>(list), default_value) {}
	explicit EnumMap(const T &default_value) { m_data.fill(default_value); }

	EnumMap() : m_data() {}

	// ---------------------------------------------------------------
	// ----- Functions & accessors -----------------------------------

	const T &operator[](Enum index) const { return m_data[(int)index]; }
	T &operator[](Enum index) { return m_data[(int)index]; }

	T *begin() { return m_data.data(); }
	T *end() { return m_data.data() + size(); }
	const T *begin() const { return m_data.data(); }
	const T *end() const { return m_data.data() + size(); }
	bool operator==(const EnumMap &rhs) const { return std::equal(begin(), end(), rhs.begin()); }
	bool operator<(const EnumMap &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	constexpr bool empty() const { return size_ == 0; }
	constexpr int size() const { return size_; }
	void fill(const T &value) {
		for(int n = 0; n < size(); n++)
			m_data[n] = value;
	}

	const T *data() const { return m_data.data(); }
	T *data() { return m_data.data(); }

	operator Span<T, size_>() { return {data(), size_}; }
	operator CSpan<T, size_>() const { return {data(), size_}; }

  private:
	array<T, size_> m_data;
};
}
