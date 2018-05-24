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

	static_assert(isEnum<Enum>(),
				  "EnumMap<> can only be used for enums specified with DEFINE_ENUM");

	static void checkPairs(CSpan<pair<Enum, T>> pairs) {
		unsigned long long flags = 0;
		static_assert(size_ <= 64);

		for(auto &pair : pairs) {
			auto bit = 1ull << int(pair.first);
			if(flags & bit)
				FATAL("Enum entry duplicated: %s", toString(pair.first));
			flags |= bit;
		}

		if(pairs.size() != size_)
			FATAL("Invalid number of entries specified: %d (expected: %d)", pairs.size(), size_);
	}

	EnumMap(CSpan<pair<Enum, T>> pairs) {
		IF_DEBUG(checkPairs(pairs));
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(CSpan<pair<Enum, T>> pairs, T default_value) {
		m_data.fill(default_value);
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(CSpan<T> values) {
		DASSERT(values.size() == size_ && "Invalid number of values specified");
		std::copy(values.begin(), values.end(), m_data.begin());
	}
	explicit EnumMap(const T &default_value) { m_data.fill(default_value); }
	EnumMap() : EnumMap(T()) {}

	EnumMap(std::initializer_list<pair<Enum, T>> list) : EnumMap(CSpan<pair<Enum, T>>(list)) {}
	EnumMap(std::initializer_list<pair<Enum, T>> list, T default_value)
		: EnumMap(CSpan<pair<Enum, T>>(list), default_value) {}
	EnumMap(std::initializer_list<T> values) : EnumMap(CSpan<T>(values)) {}

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
