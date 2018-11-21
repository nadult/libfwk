// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/light_tuple.h"
#include "fwk/math_base.h"
#include "fwk/maybe.h"

namespace fwk {

template <class T> T hashCombine(T hash1, T hash2) {
	// Source: Blender
	return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
}

template <class T, class... Args> T hashCombine(T hash1, T hash2, Args... hashes) {
	return hashCombine(hash1, hashCombine<T>(hash2, hashes...));
}

template <class Value /*= int*/> struct Hash {
	template <class T, EnableIfScalar<T>...> static Value compute(T scalar) {
		return std::hash<T>()(scalar);
	}

	template <class T, EnableIf<isRange<T>() && !has_tied_member<T>, NotARange>...>
	static Value compute(const T &range) {
		if(fwk::empty(range))
			return 0;
		auto it = begin(range);
		auto out = hash(*it);
		for(++it; it != end(range); ++it)
			out = hashCombine(out, hash(*it));
		return out;
	}

	template <class T>
	static typename std::enable_if<std::is_enum<T>::value, Value>::type compute(const T val) {
		return compute((int)val);
	}
	template <class T1, class T2> static Value compute(const pair<T1, T2> &pair) {
		return hashCombine(compute(pair.first), compute(pair.second));
	}
	template <class... Types> static Value compute(const LightTuple<Types...> &tuple) {
		return computeTuple<0>(tuple);
	}
	template <class T, EnableIf<has_tied_member<T>>...> static Value compute(const T &object) {
		return computeTuple<0>(object.tied());
	}

	template <int N, class... Types> static auto computeTuple(const LightTuple<Types...> &tuple) {
		if constexpr(N + 1 == sizeof...(Types))
			return hash(get<N>(tuple));
		else
			return hashCombine(hash(get<N>(tuple)), computeTuple<N + 1>(tuple));
	}

	template <class T> static Value compute(const Maybe<T> &maybe) {
		return maybe ? compute(*maybe) : Value(0x31337);
	}

	template <class... Types> static Value compute(const Variant<Types...> &var) {
		return hashCombine(compute(var.which()), var.visit([](auto &val) { return compute(val); }));
	}

	template <class T> Value operator()(const T &v) const { return compute(v); }
};

template <class T> int hash(const T &value) { return Hash<int>::compute(value); }
template <class V, class T> V hash(const T &value) { return Hash<V>::compute(value); }

template <class V, class... Args> V hashMany(Args &&... args) {
	return hashCombine<V>(hash<V, Args>(args)...);
}
}
