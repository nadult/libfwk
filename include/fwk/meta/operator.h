// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta_base.h"

namespace fwk {

template <class L, class R>
concept equality_comparable = requires(const L &l, const R &r) {
	{ l == r } -> is_convertible<bool>;
};

template <class L, class R>
concept less_comparable = requires(const L &l, const R &r) {
	{ l < r } -> is_convertible<bool>;
};

namespace detail {
	template <class L, class R> struct BinaryOps {
		FWK_SFINAE_TYPE(Add, L, DECLVAL(const U &) + DECLVAL(const R &));
		FWK_SFINAE_TYPE(Sub, L, DECLVAL(const U &) - DECLVAL(const R &));
		FWK_SFINAE_TYPE(Mul, L, DECLVAL(const U &) * DECLVAL(const R &));
		FWK_SFINAE_TYPE(Div, L, DECLVAL(const U &) / DECLVAL(const R &));
		FWK_SFINAE_TYPE(Or, L, DECLVAL(const U &) | DECLVAL(const R &));
		FWK_SFINAE_TYPE(And, L, DECLVAL(const U &) & DECLVAL(const R &));
		FWK_SFINAE_TYPE(Apply, L, DECLVAL(const U &)(DECLVAL(const R &)));
	};

	template <class T> struct UnaryOps {
		FWK_SFINAE_TYPE(Dereference, T, *DECLVAL(const U &));
		FWK_SFINAE_TYPE(PreIncrement, T, ++DECLVAL(U &));
	};

	template <class L, class R>
	concept can_auto_compare =
		less_comparable<L, R> && (!std::is_scalar_v<L> || !std::is_scalar_v<R>);
}

#define BINARY_OP_RESULT(name)                                                                     \
	template <class L, class R> using name##Result = typename detail::BinaryOps<L, R>::name;
#define UNARY_OP_RESULT(name)                                                                      \
	template <class T> using name##Result = typename detail::UnaryOps<T>::name;

BINARY_OP_RESULT(Add)
BINARY_OP_RESULT(Sub)
BINARY_OP_RESULT(Mul)
BINARY_OP_RESULT(Div)
BINARY_OP_RESULT(Or)
BINARY_OP_RESULT(And)
BINARY_OP_RESULT(Apply)
UNARY_OP_RESULT(Dereference)
UNARY_OP_RESULT(PreIncrement)

#undef BINARY_OP_RESULT
#undef UNARY_OP_RESULT

// Automatic operators for user types: +=, -=, *=, /=, |=, &=, <=, >=, >, !=

template <class L, class R>
	requires(is_same<AddResult<L, R>, L> && !std::is_scalar_v<L>)
void operator+=(L &a, const R &b) {
	a = a + b;
}

template <class L, class R>
	requires(is_same<SubResult<L, R>, L> && !std::is_scalar_v<L>)
void operator-=(L &a, const R &b) {
	a = a - b;
}

template <class L, class R>
	requires(is_same<MulResult<L, R>, L> && !std::is_scalar_v<L>)
void operator*=(L &a, const R &b) {
	a = a * b;
}

template <class L, class R>
	requires(is_same<DivResult<L, R>, L> && !std::is_scalar_v<L>)
void operator/=(L &a, const R &b) {
	a = a / b;
}

template <class L, class R>
	requires(is_same<OrResult<L, R>, L> && !std::is_scalar_v<L>)
void operator|=(L &a, const R &b) {
	a = a | b;
}

template <class L, class R>
	requires(is_same<AndResult<L, R>, L> && !std::is_scalar_v<L>)
void operator&=(L &a, const R &b) {
	a = a & b;
}

template <class L, class R>
	requires(detail::can_auto_compare<R, L>)
bool operator>(const L &a, const R &b) {
	return b < a;
}

template <class L, class R>
	requires(detail::can_auto_compare<L, R>)
bool operator>=(const L &a, const R &b) {
	return !(a < b);
}

template <class L, class R>
	requires(detail::can_auto_compare<R, L>)
bool operator<=(const L &a, const R &b) {
	return !(b < a);
}

template <class L, class R>
	requires(equality_comparable<L, R>)
bool operator!=(const L &a, const R &b) {
	return !(a == b);
}
}
