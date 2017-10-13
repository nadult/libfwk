// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_base.h"
#include <array>

namespace fwk {

template <class Enum, class T> class EnumMap {
  public:
	static_assert(isEnum<Enum>(),
				  "EnumMap<> can only be used for enums specified with DEFINE_ENUM");

	EnumMap(CSpan<pair<Enum, T>> pairs) {
#ifndef NDEBUG
		bool enum_used[count<Enum>()] = {
			false,
		};
		int enum_count = 0;
#endif
		for(auto &pair : pairs) {
			int index = (int)pair.first;
			m_data[index] = pair.second;
#ifndef NDEBUG
			DASSERT(!enum_used[index] && "Enum entry missing");
			enum_used[index] = true;
			enum_count++;
#endif
		}
		DASSERT(enum_count == count<Enum>() && "Invalid number of pairs specified");
	}
	EnumMap(CSpan<pair<Enum, T>> pairs, T default_value) {
		m_data.fill(default_value);
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(CSpan<T> values) {
		DASSERT(values.size() == (int)m_data.size() && "Invalid number of values specified");
		std::copy(values.begin(), values.end(), m_data.begin());
	}
	explicit EnumMap(const T &default_value = T()) { m_data.fill(default_value); }
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

	constexpr bool empty() const { return size() == 0; }
	constexpr int size() const { return count<Enum>(); }
	void fill(const T &value) {
		for(int n = 0; n < size(); n++)
			m_data[n] = value;
	}

  private:
	array<T, count<Enum>()> m_data;
};
}
