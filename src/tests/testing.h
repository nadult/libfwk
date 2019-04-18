// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/math/quat.h"
#include "fwk/parse.h"

using namespace fwk;

// TODO: ASSERT_EXCEPTION ?
#define ASSERT_FAIL(code)                                                                          \
	{                                                                                              \
		auto result = ({ code; });                                                                 \
		ASSERT(!result);                                                                           \
	}

template <class T> inline double relativeDifference(const T &a, const T &b) {
	if constexpr(is_scalar<T>) {
		auto magnitude = max(fwk::abs(a), fwk::abs(b));
		return magnitude < epsilon<T> ? T(0.0) : fwk::abs(a - b) / magnitude;
	} else if constexpr(is_same<T, Quat>) {
		return min(relativeDifference(float4(a), float4(b)),
				   relativeDifference(-float4(a), float4(b)));
	} else {
		return distance<Promote<T>>(a, b);
	}
}

template <class T, class Base = Scalar<T>>
void assertCloseEnough(const T &a, const T &b, double eps = epsilon<Base>) {
	auto diff = relativeDifference(a, b);
	if(diff > eps)
		FATAL("Not close enough: %s : %s (relative difference: %.14f > %.14f)", toString(a).c_str(),
			  toString(b).c_str(), diff, eps);
}

void testMain();

int main(int argc, char **argv) {
	testMain();
	printf("%s: OK\n", argv[0]);
	return 0;
}
