// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta_base.h"

namespace fwk {

namespace detail {
	template <class L, class R> struct BinaryOps {
		FWK_SFINAE_TYPE(Add, L, DECLVAL(const U &) + DECLVAL(const R &));
		FWK_SFINAE_TYPE(Sub, L, DECLVAL(const U &) - DECLVAL(const R &));
		FWK_SFINAE_TYPE(Mul, L, DECLVAL(const U &) * DECLVAL(const R &));
		FWK_SFINAE_TYPE(Div, L, DECLVAL(const U &) / DECLVAL(const R &));
		FWK_SFINAE_TYPE(Less, L, DECLVAL(const U &) < DECLVAL(const R &));
		FWK_SFINAE_TYPE(Equal, L, DECLVAL(const U &) == DECLVAL(const R &));
		FWK_SFINAE_TYPE(Apply, L, DECLVAL(const U &)(DECLVAL(const R &)));
	};

	template <class T> struct UnaryOps {
		FWK_SFINAE_TYPE(Dereference, T, *DECLVAL(const U &));
		FWK_SFINAE_TYPE(PreIncrement, T, ++DECLVAL(U &));
	};

	template <class L, class R>
	constexpr bool can_auto_compare = is_same<typename BinaryOps<L, R>::Less, bool> &&
									  (!std::is_scalar_v<L> || !std::is_scalar_v<R>);
}

#define BINARY_OP_RESULT(name)                                                                     \
	template <class L, class R> using name##Result = typename detail::BinaryOps<L, R>::name;
#define UNARY_OP_RESULT(name)                                                                      \
	template <class T> using name##Result = typename detail::UnaryOps<T>::name;

BINARY_OP_RESULT(Add)
BINARY_OP_RESULT(Sub)
BINARY_OP_RESULT(Mul)
BINARY_OP_RESULT(Div)
BINARY_OP_RESULT(Less)
BINARY_OP_RESULT(Equal)
BINARY_OP_RESULT(Apply)
UNARY_OP_RESULT(Dereference)
UNARY_OP_RESULT(PreIncrement)

#undef BINARY_OP_RESULT
#undef UNARY_OP_RESULT

template <class L, class R>
constexpr bool equality_comparable = is_convertible<EqualResult<L, R>, bool>;
template <class T> static constexpr bool less_comparable = is_convertible<LessResult<T, T>, bool>;

// Automatic operators for user types: +=, -=, *=, /=, <=, >=, >, !=

template <class L, class R, EnableIf<is_same<AddResult<L, R>, L> && !std::is_scalar_v<L>>...>
void operator+=(L &a, const R &b) {
	a = a + b;
}
template <class L, class R, EnableIf<is_same<SubResult<L, R>, L> && !std::is_scalar_v<L>>...>
void operator-=(L &a, const R &b) {
	a = a - b;
}
template <class L, class R, EnableIf<is_same<MulResult<L, R>, L> && !std::is_scalar_v<L>>...>
void operator*=(L &a, const R &b) {
	a = a * b;
}

template <class L, class R, EnableIf<is_same<DivResult<L, R>, L> && !std::is_scalar_v<L>>...>
void operator/=(L &a, const R &b) {
	a = a / b;
}

template <class L, class R, EnableIf<detail::can_auto_compare<R, L>>...>
bool operator>(const L &a, const R &b) {
	return b < a;
}

template <class L, class R, EnableIf<detail::can_auto_compare<L, R>>...>
bool operator>=(const L &a, const R &b) {
	return !(a < b);
}

template <class L, class R, EnableIf<detail::can_auto_compare<R, L>>...>
bool operator<=(const L &a, const R &b) {
	return !(b < a);
}

template <class L, class R, EnableIf<equality_comparable<L, R>>...>
bool operator!=(const L &a, const R &b) {
	return !(a == b);
}
}
