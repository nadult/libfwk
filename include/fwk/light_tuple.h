// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta/operator.h"
#include "fwk/sys_base.h"

namespace fwk {

namespace detail {

	template <int N> struct GetField {};
#define GET_FIELD(n)                                                                               \
	template <> struct GetField<n> {                                                               \
		template <class F> static const auto &get(const F &f) { return f.arg##n; }                 \
		template <class F> static auto &get(F &f) { return f.arg##n; }                             \
	};
	GET_FIELD(0)
	GET_FIELD(1)
	GET_FIELD(2)
	GET_FIELD(3)
	GET_FIELD(4)
	GET_FIELD(5)
	GET_FIELD(6)
	GET_FIELD(7)
	GET_FIELD(8)
	GET_FIELD(9)
#undef GET_FIELD
}

template <class... Args> struct LightTuple {
	static_assert(sizeof...(Args) == 0, "Light tuple not supported for given nr of arguments");
};

template <> struct LightTuple<> {
	enum { count = 0 };
};
template <class Arg0> struct LightTuple<Arg0> {
	enum { count = 1 };
	Arg0 arg0 = Arg0();
};
template <class Arg0, class Arg1> struct LightTuple<Arg0, Arg1> {
	enum { count = 2 };

	template <class T, class U> void operator=(const Pair<T, U> &pair) {
		arg0 = pair.first;
		arg1 = pair.second;
	}

	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
};
template <class Arg0, class Arg1, class Arg2> struct LightTuple<Arg0, Arg1, Arg2> {
	enum { count = 3 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
};
template <class Arg0, class Arg1, class Arg2, class Arg3>
struct LightTuple<Arg0, Arg1, Arg2, Arg3> {
	enum { count = 4 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4> {
	enum { count = 5 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4, Arg5> {
	enum { count = 6 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
	Arg5 arg5 = Arg5();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6> {
	enum { count = 7 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
	Arg5 arg5 = Arg5();
	Arg6 arg6 = Arg6();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6,
		  class Arg7>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7> {
	enum { count = 8 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
	Arg5 arg5 = Arg5();
	Arg6 arg6 = Arg6();
	Arg7 arg7 = Arg7();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6,
		  class Arg7, class Arg8>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8> {
	enum { count = 9 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
	Arg5 arg5 = Arg5();
	Arg6 arg6 = Arg6();
	Arg7 arg7 = Arg7();
	Arg8 arg8 = Arg8();
};

template <class Arg0, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5, class Arg6,
		  class Arg7, class Arg8, class Arg9>
struct LightTuple<Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7, Arg8, Arg9> {
	enum { count = 10 };
	Arg0 arg0 = Arg0();
	Arg1 arg1 = Arg1();
	Arg2 arg2 = Arg2();
	Arg3 arg3 = Arg3();
	Arg4 arg4 = Arg4();
	Arg5 arg5 = Arg5();
	Arg6 arg6 = Arg6();
	Arg7 arg7 = Arg7();
	Arg8 arg8 = Arg8();
	Arg9 arg9 = Arg9();
};

template <int N, class... Args> const auto &get(const LightTuple<Args...> &tuple) {
	return detail::GetField<N>::get(tuple);
}
template <int N, class... Args> auto &get(LightTuple<Args...> &tuple) {
	return detail::GetField<N>::get(tuple);
}
template <class T, class... Args> const auto &get(const LightTuple<Args...> &tuple) {
	static constexpr int tidx = type_index<T, Args...>;
	static_assert(tidx != -1, "Invalid type specified");
	return detail::GetField<tidx>::get(tuple);
}
template <class T, class... Args> auto &get(LightTuple<Args...> &tuple) {
	static constexpr int tidx = type_index<T, Args...>;
	static_assert(tidx != -1, "Invalid type specified");
	return detail::GetField<tidx>::get(tuple);
}

template <class T> static constexpr bool is_tuple = false;
template <class... T> static constexpr bool is_tuple<LightTuple<T...>> = true;

namespace detail {
	template <int TN, class LT> bool cmpLess(const LT &lhs, const LT &rhs) {
		if constexpr(LT::count > TN + 1)
			if(GetField<TN>::get(lhs) == GetField<TN>::get(rhs))
				return cmpLess<TN + 1>(lhs, rhs);
		return GetField<TN>::get(lhs) < GetField<TN>::get(rhs);
	}
	template <int TN, class LT> bool cmpEqual(const LT &lhs, const LT &rhs) {
		if(!(GetField<TN>::get(lhs) == GetField<TN>::get(rhs)))
			return false;
		if constexpr(LT::count > TN + 1)
			return cmpEqual<TN + 1>(lhs, rhs);
		return true;
	}

	template <class T> struct HasTiedMember {
		template <class C> static auto test(int) -> decltype(DECLVAL(C &).tied());
		template <class C> static void test(...);
		static constexpr bool value = is_tuple<decltype(test<T>(0))>;
	};
}

template <class... Args>
constexpr bool operator<(const LightTuple<Args...> &lhs, const LightTuple<Args...> &rhs) {
	static_assert((less_comparable<Args> && ...));
	return detail::cmpLess<0>(lhs, rhs);
}

template <class... Args>
constexpr bool operator==(const LightTuple<Args...> &lhs, const LightTuple<Args...> &rhs) {
	static_assert((equality_comparable<Args, Args> && ...));
	return detail::cmpEqual<0>(lhs, rhs);
}

template <class... Args> constexpr auto tie(const Args &...args) {
	return LightTuple<const Args &...>{args...};
}

template <class... Args> constexpr auto tuple(Args &&...args) {
	return LightTuple<Decay<RemoveReference<Args>>...>{std::forward<Args>(args)...};
}

// Do not use it with bitfields! It will make a const ref to temporary
// Unfortunately there is no good way to protect yourself from this...
#define FWK_TIE_MEMBERS(...)                                                                       \
	auto tied() const { return fwk::tie(__VA_ARGS__); }

#define FWK_TIED_COMPARES(name)                                                                    \
	bool operator==(const name &rhs) const { return tied() == rhs.tied(); }                        \
	bool operator<(const name &rhs) const { return tied() < rhs.tied(); }

#define FWK_ORDER_BY(name, ...)                                                                    \
	FWK_TIE_MEMBERS(__VA_ARGS__)                                                                   \
	FWK_TIED_COMPARES(name)

#define FWK_ORDER_BY_DEF(name, ...)                                                                \
	auto name::tied() const { return fwk::tie(__VA_ARGS__); }                                      \
	bool name::operator==(const name &rhs) const { return tied() == rhs.tied(); }                  \
	bool name::operator<(const name &rhs) const { return tied() < rhs.tied(); }

#define FWK_ORDER_BY_DECL(name)                                                                    \
	auto tied() const;                                                                             \
	bool operator==(const name &) const;                                                           \
	bool operator<(const name &) const;

struct NoTiedMember;

template <class T> constexpr bool has_tied_member = detail::HasTiedMember<T>::value;
}
