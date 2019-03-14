// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/math_base.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class T> struct EnumFlags {
	static constexpr int count = fwk::count<T>();
	using Base = If<count <= 8, u8, If<count <= 16, u16, If<count <= 32, u32, u64>>>;
	using BigBase = If<(count <= 32), u32, u64>;

	static constexpr Base mask = (Base(1) << (count - 1)) - Base(1) + (Base(1) << (count - 1));

	static_assert(is_enum<T>, "EnumFlags<> should be based on fwk-enum");
	static_assert(std::is_unsigned<Base>::value, "");
	static_assert(count <= sizeof(Base) * 8, "");

	struct BitIter {
		int bit;
		BigBase bits;

		T operator*() const { return PASSERT(bit >= 0 && bit < count), T(bit); }
		bool operator==(const BitIter &rhs) const { return bit == rhs.bit; }
		bool operator<(const BitIter &rhs) const { return bit < rhs.bit; }

		const BitIter &operator++() {
			bit++;
			if(!(bits & (BigBase(1) << bit)))
				bit = min(count, bit + countTrailingZeros(bits >> bit));
			return *this;
		}
	};

	constexpr EnumFlags() : bits(0) {}
	constexpr EnumFlags(None) : bits(0) {}
	constexpr EnumFlags(T value) : bits(Base(1) << uint(value)) {}
	constexpr explicit EnumFlags(Base bits) : bits(bits) {}

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

	constexpr operator bool() const { return bits != 0; }

	auto begin() const { return BitIter{bits & 1 ? 0 : countTrailingZeros(BigBase(bits)), bits}; }
	auto end() const { return BitIter{count, bits}; }

	void setIf(EnumFlags flags, bool condition) {
		if(condition)
			bits |= flags.bits;
		else
			bits &= ~(flags.bits);
	}

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

template <class T> constexpr EnumFlags<T> operator|(T lhs, EnumFlags<T> rhs) { return rhs | lhs; }
template <class T> constexpr EnumFlags<T> operator&(T lhs, EnumFlags<T> rhs) { return rhs & lhs; }
template <class T> constexpr EnumFlags<T> operator^(T lhs, EnumFlags<T> rhs) { return rhs ^ lhs; }

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator~(T bit) {
	return ~EnumFlags<T>(bit);
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> mask(bool cond, T val) {
	return cond ? val : EnumFlags<T>();
}

template <class C, class T, EnableIf<is_convertible<C, bool>>...>
constexpr EnumFlags<T> mask(C cond, EnumFlags<T> val) {
	return cond ? val : EnumFlags<T>();
}

template <class T> constexpr bool is_enum_flags = false;
template <class T> constexpr bool is_enum_flags<EnumFlags<T>> = true;

template <class T> constexpr int countBits(const EnumFlags<T> &flags) {
	return countBits(typename EnumFlags<T>::BigBase(flags.bits));
}

template <class T> TextParser &operator>>(TextParser &parser, EnumFlags<T> &value) {
	using Strings = decltype(enumStrings(T()));
	value.bits = fwk::detail::parseFlags(parser, Strings::offsets.data, Strings::K);
	return parser;
}

template <class T> TextFormatter &operator<<(TextFormatter &formatter, EnumFlags<T> value) {
	using Strings = decltype(enumStrings(T()));
	fwk::detail::formatFlags(value.bits, formatter, Strings::offsets.data, Strings::K);
	return formatter;
}
}
