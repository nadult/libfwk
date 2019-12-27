// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/hash.h"
#include "fwk/math_base.h"

namespace fwk {

#ifndef FWK_TARGET_HTML
template <class H> H computeHash(const uint128 &value, PriorityTag0) {
	return combineHash<H>(computeHash<H>(u64(value)), computeHash<H>(u64(value >> 64)));
}
template <class H> H computeHash(const int128 &value, PriorityTag0) {
	return combineHash<H>(computeHash<H>(i64(value)), computeHash<H>(i64(value >> 64)));
}
#endif

TextFormatter &operator<<(TextFormatter &, int128);
TextFormatter &operator<<(TextFormatter &, uint128);
}
