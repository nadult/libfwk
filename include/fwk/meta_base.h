// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <type_traits>

namespace fwk {

struct EnabledType {};
struct DisabledType;
struct InvalidType;

struct Empty {};

struct None {
	bool operator==(const None &) const { return true; }
	bool operator<(const None &) const { return false; }
};
constexpr None none = {};

template <class T, int N> struct IndexedType {
	using type = T;
	static constexpr int value = N;
};

template <class T> struct Type {};
template <class... Args> struct Types {};

template <class T> struct UnwrapType_ { using Type = T; };
template <class T> struct UnwrapType_<Type<T>> { using Type = T; };
template <class T> using UnwrapType = typename UnwrapType_<T>::Type;

template <class T> auto declval() -> T;
#define DECLVAL(type) declval<type>()

template <class Lhs, class... RhsArgs> constexpr bool is_one_of = false;
template <class Lhs, class Rhs, class... RhsArgs>
constexpr bool is_one_of<Lhs, Rhs, RhsArgs...> = is_one_of<Lhs, RhsArgs...>;
template <class Lhs, class... RhsArgs> constexpr bool is_one_of<Lhs, Lhs, RhsArgs...> = true;

template <bool value, class T1, class T2> using If = typename std::conditional<value, T1, T2>::type;

template <class Base, class Derived>
constexpr bool is_base_of = std::is_base_of<Base, Derived>::value;

template <class T1, class T2> constexpr bool is_same = false;
template <class T> constexpr bool is_same<T, T> = true;

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

	template <class TFrom, class TWhat> struct SubTypes;
	template <class... VWhat> struct SubTypes<Types<>, Types<VWhat...>> { using Type = Types<>; };

	template <class From, class... VFrom, class... VWhat>
	struct SubTypes<Types<From, VFrom...>, Types<VWhat...>> {
		using Rest = typename SubTypes<Types<VFrom...>, Types<VWhat...>>::Type;

		template <class... VRest>
		static constexpr auto result(Types<VRest...>)
			-> If<is_one_of<From, VWhat...>, Rest, Types<From, VRest...>>;
		using Type = decltype(result(Rest()));
	};

	template <class T, class... Args> struct IsConstr {
		template <class... UArgs, class U>
		static constexpr auto test(U *) -> decltype(U{DECLVAL(UArgs)...});
		template <class... UArgs> static constexpr void test(...);
		static constexpr bool value = !is_same<decltype(test<Args...>(DECLVAL(T *))), void>;
	};
}

template <class TFrom, class TWhat>
using SubtractTypes = typename detail::SubTypes<TFrom, TWhat>::Type;

template <int N, class... Args> using NthType = typename detail::NthType<N, Args...>::type;

template <bool cond, class InvalidArg = DisabledType>
using EnableIf =
	typename std::conditional<cond, detail::ValidType, InvalidArg>::type::template Arg<EnabledType>;

template <class T1, class T2> constexpr bool is_convertible = std::is_convertible<T1, T2>::value;

template <class T, class... Args>
constexpr bool is_constructible = detail::IsConstr<T, Args...>::value;

struct NotConstructible;
template <class T, class... Args>
using EnableIfConstructible = EnableIf<is_constructible<T, Args...>, NotConstructible>;

template <class T1> constexpr bool is_const = false;
template <class T> constexpr bool is_const<const T> = true;

template <class T> using RemoveConst = typename std::remove_const<T>::type;
template <class T> using RemoveReference = typename std::remove_reference<T>::type;
template <class T> using RemovePointer = typename std::remove_pointer<T>::type;
template <class T>
using RemoveRefConst = typename std::remove_const<typename std::remove_reference<T>::type>::type;

// TODO: template variables
using detail::Conjunction;
using detail::Disjunction;

template <class... Types> using IndexedTypes = typename detail::MakeIndexedTypes<0, Types...>::type;

template <class... Args> constexpr int type_count = sizeof...(Args);
template <class... Args> constexpr int type_count<Types<Args...>> = sizeof...(Args);

// In case of multiple types, returns first; If not found: -1
template <class Arg, class... Args>
constexpr int type_index = []() {
	int result = -1, idx = 0;
	(..., ((is_same<Arg, Args> &&result == -1 ? result = idx : 0), idx++));
	return result;
}();
template <class Arg, class... Args>
constexpr int type_index<Arg, Types<Args...>> = type_index<Arg, Args...>;

template <class... Args>
constexpr bool unique_types = []() {
	bool unique = true;
	int idx = 0;
	(..., ((type_index<Args, Args...> != idx ? unique = false : 0), idx++));
	return unique;
}();
template <class... Args> constexpr bool unique_types<Types<Args...>> = unique_types<Args...>;

template <class T> constexpr int type_size = sizeof(T);

template <class T> using Decay = std::decay_t<T>;
}
