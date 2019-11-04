// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/algorithm.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: erase or remove? What's the best naming?
template <class T> void remove(vector<T> &vec, const T &value) {
	auto it = std::remove_if(begin(vec), end(vec), [&](const T &ref) { return ref == value; });
	vec.erase(it, vec.end());
}

template <class T, class Func> void removeIf(vector<T> &vec, const Func &func) {
	auto it = std::remove_if(begin(vec), end(vec), func);
	vec.erase(it, vec.end());
}
}
