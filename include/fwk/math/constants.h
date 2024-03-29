// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(NumberType, integral, rational, real, infinity);

template <NumberType type = NumberType::real> struct RealConstant {
	constexpr RealConstant(long double val, bool sign = false) : val(val), sign(sign) {}
	template <class T, EnableIf<is_scalar<T>>...> constexpr operator T() const {
		return sign ? -T(val) : T(val);
	}
	constexpr RealConstant operator-() const { return {val, !sign}; }

#define IF_FPT template <class T, EnableIf<is_fpt<T>>...>

	IF_FPT constexpr T operator*(T val) const { return T(*this) * val; }
	IF_FPT constexpr T operator/(T val) const { return T(*this) / val; }
	IF_FPT constexpr T operator-(T val) const { return T(*this) - val; }
	IF_FPT constexpr T operator+(T val) const { return T(*this) + val; }
	IF_FPT constexpr bool operator<(T val) const { return T(*this) < val; }

	IF_FPT friend constexpr T operator*(T v, const RealConstant &rc) { return v * T(rc); }
	IF_FPT friend constexpr T operator/(T v, const RealConstant &rc) { return v / T(rc); }
	IF_FPT friend constexpr T operator-(T v, const RealConstant &rc) { return v - T(rc); }
	IF_FPT friend constexpr T operator+(T v, const RealConstant &rc) { return v + T(rc); }

	IF_FPT friend constexpr bool operator==(T v, const RealConstant &rc) { return v == T(rc); }
	IF_FPT friend constexpr bool operator<(T v, const RealConstant &rc) { return v < T(rc); }

#undef IF_FPT

	long double val;
	bool sign;
};

constexpr RealConstant sqrt2 = 1.4142135623730950488016887242096981L;
constexpr RealConstant sqrt3 = 1.7320508075688772935274463415058724L;
constexpr RealConstant sqrt6 = 2.4494897427831780981972840747058914L;
constexpr RealConstant pi = 3.1415926535897932384626433832795029L;
constexpr RealConstant e = 2.7182818284590452353602874713526625L;
constexpr RealConstant<NumberType::infinity> inf = __builtin_huge_valf();
}
