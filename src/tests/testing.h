// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/math/quat.h"
#include "fwk/parse.h"
#include "fwk/sys/rollback.h"

using namespace fwk;

#define ASSERT_FAIL(code)                                                                          \
	{                                                                                              \
		auto result = RollbackContext::begin([&]() { code; });                                     \
		ASSERT(!result);                                                                           \
	}

inline float relativeDifference(float a, float b) {
	float magnitude = max(fabs(a), fabs(b));
	return magnitude < fconstant::epsilon ? 0.0f : fabs(a - b) / magnitude;
}

inline bool closeEnough(float a, float b) { return relativeDifference(a, b) < fconstant::epsilon; }
template <class T> inline bool closeEnough(const T &a, const T &b) {
	return distance(a, b) < fconstant::epsilon;
}

template <class T> void reportError(const T &a, const T &b) {
	FATAL("Error:  %s != %s", toString(a).c_str(), toString(b).c_str());
}

template <class T> void assertCloseEnough(const T &a, const T &b) {
	if(!closeEnough(a, b))
		reportError(a, b);
}

inline void assertCloseEnough(const Quat &a, const Quat &b) {
	if(!closeEnough((float4)a, (float4)b) && !closeEnough(-(float4)a, (float4)b))
		reportError(a, b);
}

void testMain();

int main(int argc, char **argv) {
	testMain();
	printf("%s: OK\n", argv[0]);
	return 0;
}
