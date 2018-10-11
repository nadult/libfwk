// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <type_traits>

namespace fwk {

struct EnabledType {};
struct DisabledType;

struct Empty {};

struct None {
	bool operator==(const None &) const { return true; }
	bool operator<(const None &) const { return false; }
};
constexpr None none = {};

template <class T, int N> struct IndexedType {
	using type = T;
	enum { value = N };
};

template <class T> struct Type {};
template <class... Args> struct Types {};

template <class T> auto declval() -> T;

struct NoAssertsTag {};
constexpr NoAssertsTag no_asserts_tag;

namespace detail {

	struct ValidType {
		template <class A> using Arg = A;
	};

	template <int N, typename T, typename... Types> struct NthType {
		using type = typename NthType<N - 1, Types...>::type;
	};
	template <typename T, typename... Types> struct NthType<0, T, Types...> { using type = T; };

	template <class...> struct Disjunction : std::false_type {};
	template <class B1> struct Disjunction<B1> : B1 {};
	template <class B1, class... Bn>
	struct Disjunction<B1, Bn...> : std::conditional_t<bool(B1::value), B1, Disjunction<Bn...>> {};

	template <class...> struct Conjunction : std::true_type {};
	template <class B1> struct Conjunction<B1> : B1 {};
	template <class B1, class... Bn>
	struct Conjunction<B1, Bn...> : std::conditional_t<bool(B1::value), Conjunction<Bn...>, B1> {};

	template <class Arg0, class ArgN> struct MergeTypes { using type = Types<Arg0, ArgN>; };
	template <class Arg0, class... Args> struct MergeTypes<Arg0, Types<Args...>> {
		using type = Types<Arg0, Args...>;
	};

	template <int N, class... Args> struct MakeIndexedTypes { using type = Types<>; };

	template <int N, class Arg0, class... Args> struct MakeIndexedTypes<N, Arg0, Args...> {
		using rest = typename MakeIndexedTypes<N + 1, Args...>::type;
		using type = typename MergeTypes<IndexedType<Arg0, N>, rest>::type;
	};

	template <unsigned...> struct Seq { using type = Seq; };
	template <unsigned N, unsigned... Is> struct GenSeqX : GenSeqX<N - 1, N - 1, Is...> {};
	template <unsigned... Is> struct GenSeqX<0, Is...> : Seq<Is...> {};
	template <unsigned N> using GenSeq = typename GenSeqX<N>::type;
}

template <class Lhs, class... RhsArgs> struct IsOneOf { static constexpr bool value = false; };
template <class Lhs, class Rhs, class... RhsArgs> struct IsOneOf<Lhs, Rhs, RhsArgs...> {
	static constexpr bool value = IsOneOf<Lhs, RhsArgs...>::value;
};
template <class Lhs, class... RhsArgs> struct IsOneOf<Lhs, Lhs, RhsArgs...> {
	static constexpr bool value = true;
};

template <int N, class... Args> using NthType = typename detail::NthType<N, Args...>::type;

template <bool cond, class InvalidArg = DisabledType>
using EnableIf =
	typename std::conditional<cond, detail::ValidType, InvalidArg>::type::template Arg<EnabledType>;

template <class T1, class T2> constexpr bool isSame() { return std::is_same<T1, T2>::value; }
template <class T> constexpr bool isConst() { return std::is_const<T>::value; }

template <bool value, class T1, class T2>
using Conditional = typename std::conditional<value, T1, T2>::type;

template <class T> using RemoveConst = typename std::remove_const<T>::type;
template <class T> using RemoveReference = typename std::remove_reference<T>::type;

// TODO: template variables
using detail::Conjunction;
using detail::Disjunction;

template <class... Types> using IndexedTypes = typename detail::MakeIndexedTypes<0, Types...>::type;

template <class T> constexpr int type_size = sizeof(T);

template <class T> using Decay = std::decay_t<T>;
}
