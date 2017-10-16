// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/light_tuple.h"
#include "fwk/math_base.h"

namespace fwk {

template <class Value /*= int*/> struct Hash {
	// Source: Blender
	static Value hashCombine(Value hash_a, Value hash_b) {
		return hash_a ^ (hash_b + 0x9e3779b9 + (hash_a << 6) + (hash_a >> 2));
	}

	template <class T, EnableIfScalar<T>...> static Value hash(T scalar) {
		return std::hash<T>()(scalar);
	}

	template <class T, EnableIf<isRange<T>() && !isTied<T>(), NotARange>...>
	static Value hash(const T &range) {
		if(fwk::empty(range))
			return 0;
		auto it = begin(range);
		auto out = hash(*it);
		for(++it; it != end(range); ++it)
			out = hashCombine(out, hash(*it));
		return out;
	}

	template <class T>
	static typename std::enable_if<std::is_enum<T>::value, Value>::type hash(const T val) {
		return hash((int)val);
	}
	template <class T1, class T2> static Value hash(const std::pair<T1, T2> &pair) {
		return hashCombine(hash(pair.first), hash(pair.second));
	}
	template <class... Types> static Value hash(const LightTuple<Types...> &tuple) {
		return hashTuple<0>(tuple);
	}
	template <class T, EnableIfTied<T>...> static Value hash(const T &object) {
		return hashTuple<0>(object.tied());
	}

	template <int N, class... Types> static auto hashTuple(const LightTuple<Types...> &tuple) {
		if constexpr(N + 1 == sizeof...(Types))
			return hash(get<N>(tuple));
		else
			return hashCombine(hash(get<N>(tuple)), hashTuple<N + 1>(tuple));
	}

	template <class T> auto operator()(const T &value) const { return hash(value); }
};

template <class T> int hash(const T &value) { return Hash<int>()(value); }
}
