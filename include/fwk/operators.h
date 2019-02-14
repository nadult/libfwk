// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta.h"

namespace fwk {

namespace detail {
#define OP_RESULT(name, op)                                                                        \
	template <class L, class R> struct name##Result {                                              \
		template <class U> static decltype(DECLVAL(const U &) op DECLVAL(const R &)) test(U *);    \
		template <class U> static void test(...);                                                  \
		using Type = decltype(test<L>(nullptr));                                                   \
	};
	OP_RESULT(Add, +)
	OP_RESULT(Sub, -)
	OP_RESULT(Mul, *)
	OP_RESULT(Div, /)
	OP_RESULT(Less, <)
	OP_RESULT(Equal, ==)
#undef OP_RESULT
	template <class L, class R> struct ApplyResult {
		template <class U> static decltype(DECLVAL(const U &)(DECLVAL(const R &))) test(U *);
		template <class U> static void test(...);
		using Type = decltype(test<L>(nullptr));
	};

	template <class L, class R>
	static constexpr bool can_auto_compare = is_same<typename LessResult<L, R>::Type, bool> &&
											 (!std::is_scalar_v<L> || !std::is_scalar_v<R>);
}

#define OP_RESULT(name)                                                                            \
	template <class L, class R> using name##Result = typename detail::name##Result<L, R>::Type;
OP_RESULT(Add)
OP_RESULT(Sub)
OP_RESULT(Mul)
OP_RESULT(Div)
OP_RESULT(Less)
OP_RESULT(Equal)
OP_RESULT(Apply)
#undef OP_RESULT

// Automatic operators for user types: +=, -=, *=, /=, <=, >=, >, !=

template <class T1, class T2> bool operator!=(const T1 &a, const T2 &b) { return !(a == b); }

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
}
