/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#ifndef TESTING_H
#define TESTING_H

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_xml.h"
#include <cmath>

using namespace fwk;

#define ASSERT_EXCEPTION(code)                                                                     \
	{                                                                                              \
		bool exception_thrown = false;                                                             \
		try {                                                                                      \
			code;                                                                                  \
		} catch(...) { exception_thrown = true; }                                                  \
		ASSERT(exception_thrown);                                                                  \
	}

inline float relativeDifference(float a, float b) {
	float magnitude = max(fabs(a), fabs(b));
	return magnitude < constant::epsilon ? 0.0f : fabs(a - b) / magnitude;
}

inline bool closeEnough(float a, float b) { return relativeDifference(a, b) < constant::epsilon; }
template <class T> inline bool closeEnough(const T &a, const T &b) {
	return distance(a, b) < constant::epsilon;
}

template <class T> void assertCloseEnough(const T &a, const T &b) {
	if(!closeEnough(a, b)) {
		TextFormatter atext, btext;
		xml_conversions::toString(a, atext);
		xml_conversions::toString(b, btext);
		THROW("Error:  %s != %s", atext.text(), btext.text());
	}
}

template <class T> void assertEqual(const T &a, const T &b) {
	if(!(a == b)) {
		TextFormatter atext, btext;
		xml_conversions::toString(a, atext);
		xml_conversions::toString(b, btext);
		THROW("Error:  %s != %s", atext.text(), btext.text());
	}
}

void testMain();

int main(int argc, char **argv) {
	try {
		testMain();
		printf("%s: OK\n", argv[0]);
	} catch(const Exception &ex) {
		printf("%s: FAILED\n%s\nBacktrace:\n%s\n", argv[0], ex.what(),
			   cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
	return 0;
}

#endif
