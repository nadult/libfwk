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

#define IF_REAL template <class T, EnableIf<is_real<T>>...>

	IF_REAL constexpr T operator*(T val) const { return T(*this) * val; }
	IF_REAL constexpr T operator/(T val) const { return T(*this) / val; }
	IF_REAL constexpr T operator-(T val) const { return T(*this) - val; }
	IF_REAL constexpr T operator+(T val) const { return T(*this) + val; }
	IF_REAL constexpr bool operator<(T val) const { return T(*this) < val; }

	IF_REAL friend constexpr T operator*(T v, const RealConstant &rc) { return v * T(rc); }
	IF_REAL friend constexpr T operator/(T v, const RealConstant &rc) { return v / T(rc); }
	IF_REAL friend constexpr T operator-(T v, const RealConstant &rc) { return v - T(rc); }
	IF_REAL friend constexpr T operator+(T v, const RealConstant &rc) { return v + T(rc); }
	IF_REAL friend constexpr bool operator<(T v, const RealConstant &rc) { return v < T(rc); }

#undef IF_REAL

	long double val;
	bool sign;
};

static constexpr RealConstant sqrt2 = 1.4142135623730950488016887242096981L;
static constexpr RealConstant sqrt3 = 1.7320508075688772935274463415058724L;
static constexpr RealConstant sqrt6 = 2.4494897427831780981972840747058914L;
static constexpr RealConstant pi = 3.1415926535897932384626433832795029L;
static constexpr RealConstant e = 2.7182818284590452353602874713526625L;
static constexpr RealConstant<NumberType::infinity> inf = __builtin_inf();
}
