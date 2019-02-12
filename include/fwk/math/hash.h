// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/light_tuple.h"
#include "fwk/math_base.h"
#include "fwk/maybe.h"

namespace fwk {

template <class H> H combineHash(H hash1, H hash2) {
	// Source: Blender
	return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
}

template <class H, class... Args> H combineHash(H hash1, H hash2, Args... hashes) {
	return combineHash(hash1, combineHash<H>(hash2, hashes...));
}

template <class H, class T> H computeHash(const T &value) {
	return computeHash<H>(value, PriorityTagMax());
}

// Different hashes have different application priorities (lower is less important)
// you can use them to properly specialize for your classes

template <class H, class T> H computeHash(const T &value, PriorityTag0) {
	return H(std::hash<T>()(value));
}

template <class H, class TRange, EnableIf<is_range<TRange>>...>
H computeHash(const TRange &range, PriorityTag1) {
	if(fwk::empty(range))
		return 0x31337;
	auto it = begin(range);
	H out = computeHash<H>(*it);
	for(++it; it != end(range); ++it)
		out = combineHash(out, computeHash<H>(*it));
	return out;
}

template <class H, class T1, class T2> H computeHash(const Pair<T1, T2> &pair, PriorityTag1) {
	return combineHash(computeHash<H>(pair.first), computeHash<H>(pair.second));
}

template <class H, class T> H computeHash(const Maybe<T> &value, PriorityTag1) {
	return value ? computeHash<H>(*value) : H(0x31337);
}
template <class H, class... Types> H computeHash(const Variant<Types...> &var, PriorityTag1) {
	return combineHash(computeHash<H>(var.which()),
					   var.visit([](auto &v) { return computeHash<H>(v); }));
}

template <class H, int N, class... Types> auto computeTupleHash(const LightTuple<Types...> &tuple) {
	if constexpr(N + 1 == sizeof...(Types))
		return computeHash<H>(get<N>(tuple));
	else
		return combineHash(computeHash<H>(get<N>(tuple)), computeTupleHash<H, N + 1>(tuple));
}

template <class H, class... Types> H computeHash(const LightTuple<Types...> &tuple, PriorityTag1) {
	return computeTupleHash<H, 0>(tuple);
}

template <class H, class T, EnableIf<has_tied_member<T>>...>
H computeHash(const T &object, PriorityTag2) {
	return computeTupleHash<H, 0>(object.tied());
}

template <class H, class T, class Ret = decltype(DECLVAL(const T &).hash()),
		  EnableIfIntegral<Ret>...>
H computeHash(const T &object, PriorityTag3) {
	return object.hash();
}

template <class H, class T, class Ret = decltype(DECLVAL(const T &).template hash<H>()),
		  EnableIf<is_same<Ret, H>>...>
H computeHash(const T &object, PriorityTag4) {
	return object.template hash<H>();
}

template <class T> unsigned hash(const T &value) { return computeHash<unsigned>(value); }
template <class H, class T> H hash(const T &value) { return computeHash<H>(value); }

template <class H, class... Args> H hashMany(Args &&... args) {
	return combineHash(hash<H, Args>(args)...);
}
}
