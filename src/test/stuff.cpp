/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "testing.h"

void testTextFormatter() {
	TextFormatter fmt;
	fmt("%d %x %s", 11, 0x20, "foobar");
	ASSERT(fmt.text() == string("11 20 foobar"));
}

void testXMLConverters() {
	using namespace xml_conversions;

	ASSERT(TextParser("1 2 aa bb cc 4d").countElements() == 6);

	ASSERT(fromString<vector<int>>("1 2 3 4 5") == vector<int>({1, 2, 3, 4, 5}));
	ASSERT(fromString<FRect>("1 1 4 4") == FRect(1, 1, 4, 4));
	ASSERT(fromString<float2>("100   1") == float2(100, 1));

	ASSERT(fromString<vector<float2>>("1 2 4 5.5") == vector<float2>({{1, 2}, {4, 5.5}}));
	ASSERT(fromString<bool>("true") == true);
	ASSERT(fromString<bool>("false") == false);

	ASSERT(toString(vector<int>({4, 5, 6, 7, 8})).text() == string("4 5 6 7 8"));
	ASSERT(toString(vector<float>({1, 2, 3, 4.5, 5.5, 6})).text() == string("1 2 3 4.5 5.5 6"));
	ASSERT_EXCEPTION(fromString<vector<int>>("1 2a 3"));
	ASSERT_EXCEPTION(fromString<bool>("foobar"));
}

void testMain() {
	testTextFormatter();
	testXMLConverters();
}
