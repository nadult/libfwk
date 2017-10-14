// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class T, class Base_> struct EnumFlags {
	using Base = Base_;
	enum { max_flags = fwk::count<T>() };
	static constexpr const Base mask =
		(Base(1) << (max_flags - 1)) - Base(1) + (Base(1) << (max_flags - 1));

	static_assert(isEnum<T>(), "EnumFlags<> should be based on fwk-enum");
	static_assert(std::is_unsigned<Base>::value, "");
	static_assert(fwk::count<T>() <= sizeof(Base) * 8, "");

	constexpr EnumFlags() : bits(0) {}
	constexpr EnumFlags(None) : bits(0) {}
	constexpr EnumFlags(T value) : bits(Base(1) << uint(value)) {}
	constexpr explicit EnumFlags(Base bits) : bits(bits) {}
	template <class TBase> constexpr EnumFlags(EnumFlags<T, TBase> rhs) : bits(rhs.bits) {}

	constexpr EnumFlags operator|(EnumFlags rhs) const { return EnumFlags(bits | rhs.bits); }
	constexpr EnumFlags operator&(EnumFlags rhs) const { return EnumFlags(bits & rhs.bits); }
	constexpr EnumFlags operator^(EnumFlags rhs) const { return EnumFlags(bits ^ rhs.bits); }
	constexpr EnumFlags operator~() const { return EnumFlags((~bits) & (mask)); }

	constexpr void operator|=(EnumFlags rhs) { bits |= rhs.bits; }
	constexpr void operator&=(EnumFlags rhs) { bits &= rhs.bits; }
	constexpr void operator^=(EnumFlags rhs) { bits ^= rhs.bits; }

	constexpr bool operator==(T rhs) const { return bits == (Base(1) << int(rhs)); }
	constexpr bool operator==(EnumFlags rhs) const { return bits == rhs.bits; }
	constexpr bool operator<(EnumFlags rhs) const { return bits < rhs.bits; }

	constexpr explicit operator bool() const { return bits != 0; }

	static constexpr EnumFlags all() { return EnumFlags(mask); }

	Base bits;
};

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> flag(T val) {
	return EnumFlags<T>(val);
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator|(T lhs, T rhs) {
	return EnumFlags<T>(lhs) | rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator&(T lhs, T rhs) {
	return EnumFlags<T>(lhs) & rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator^(T lhs, T rhs) {
	return EnumFlags<T>(lhs) ^ rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator~(T bit) {
	return ~EnumFlags<T>(bit);
}

namespace detail {
	template <class T> struct IsEnumFlags {
		enum { value = 0 };
	};
	template <class T, class Base> struct IsEnumFlags<EnumFlags<T, Base>> {
		enum { value = 1 };
	};
}

template <class T> constexpr bool isEnumFlags() { return detail::IsEnumFlags<T>::value; }

template <class T> using EnableIfEnumFlags = EnableIf<detail::IsEnumFlags<T>::value>;

template <class TFlags, EnableIfEnumFlags<TFlags>...> constexpr int countBits(const TFlags &flags) {
	int out = 0;
	for(int i = 0; i < TFlags::max_flags; i++)
		out += flags.bits & (typename TFlags::Base(1) << i) ? 1 : 0;
	return out;
}
}
