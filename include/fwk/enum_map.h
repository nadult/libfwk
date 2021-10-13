// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/span.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class Enum, class T> class EnumMap {
  public:
	static constexpr int size_ = count<Enum>;

	static_assert(is_enum<Enum>,
				  "EnumMap<> can only be used for enums specified with DEFINE_ENUM*");

	static void checkPairs(CSpan<Pair<Enum, T>> pairs) {
		u64 flags[(size_ + 63) / 64] = { 0, };

		for(auto &pair : pairs) {
			int offset = int(pair.first) >> 6;
			auto bit = 1ull << (int(pair.first) & 63);
			if(flags[offset] & bit)
				FWK_FATAL("Enum entry duplicated: %s", toString(pair.first));
			flags[offset] |= bit;
		}

		if(pairs.size() != size_)
			FWK_FATAL("Invalid number of entries specified: %d (expected: %d)", pairs.size(),
					  size_);
	}

	// ---------------------------------------------------------------
	// ----- Initializers with all values specified ------------------

	template <class U, int N, EnableIf<is_convertible<U, T>>...> EnumMap(CSpan<U, N> values) {
		DASSERT(values.size() == size_ && "Invalid number of values specified");
		std::copy(values.begin(), values.end(), m_data);
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
		std::fill(m_data, m_data + size_, default_value);
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(std::initializer_list<Pair<Enum, T>> list, T default_value)
		: EnumMap(CSpan<Pair<Enum, T>>(list), default_value) {}
	explicit EnumMap(const T &default_value) { std::fill(m_data, m_data + size_, default_value); }

	EnumMap() : m_data() {}

	// ---------------------------------------------------------------
	// ----- Functions & accessors -----------------------------------

	const T &operator[](Enum index) const {
		PASSERT(uint(index) < (uint)size_);
		return m_data[(int)index];
	}
	T &operator[](Enum index) {
		PASSERT(uint(index) < (uint)size_);
		return m_data[(int)index];
	}

	T *begin() { return m_data; }
	T *end() { return m_data + size_; }
	const T *begin() const { return m_data; }
	const T *end() const { return m_data + size_; }
	bool operator==(const EnumMap &rhs) const { return std::equal(begin(), end(), rhs.begin()); }
	bool operator<(const EnumMap &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	constexpr bool empty() const { return size_ == 0; }
	constexpr int size() const { return size_; }
	void fill(const T &value) {
		for(int n = 0; n < size_; n++)
			m_data[n] = value;
	}

	const T *data() const { return m_data; }
	T *data() { return m_data; }

	operator Span<T, size_>() { return {m_data, size_}; }
	operator CSpan<T, size_>() const { return {m_data, size_}; }

  private:
	T m_data[size_];
};
}
