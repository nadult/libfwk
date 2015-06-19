/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "testing.h"

void testTextFormatter() {
	TextFormatter fmt;
	fmt("%d %x %s", 11, 0x20, "foobar");
	ASSERT(fmt.text() == string("11 20 foobar"));
}

template <class T> void testClassConversions(T value) {
	using namespace xml_conversions;

	string str = toString(value).text();
	ASSERT(fromString<T>(str.c_str()) == value);

	vector<T> vec = {value, value, value, value};
	string vec_str = toString(vec).text();
	ASSERT(fromString<vector<T>>(vec_str.c_str()) == vec);
}

void testXMLConverters() {
	using namespace xml_conversions;

	ASSERT(TextParser("1 2 aa bb cc 4d").countElements() == 6);

	testClassConversions(99);
	testClassConversions(1234.5f);
	testClassConversions(568u);
	testClassConversions(false);
	testClassConversions(true);
	testClassConversions(string("foobar"));

	testClassConversions(int2(4, 5));
	testClassConversions(int3(6, 7, 99));
	testClassConversions(int4(10, 11, 1000, 11));
	testClassConversions(float2(11, 17));
	testClassConversions(float3(45, 67, 1.5));
	testClassConversions(float4(1, 1.5, 5.5, 12.5));

	testClassConversions(IRect(1, 2, 3, 4));
	testClassConversions(FRect(0, 0.5, 2, 2.5));
	testClassConversions(IBox(0, 0, 0, 30, 30, 30));
	testClassConversions(FBox(10, 20, 30, 100, 200, 300));
	testClassConversions(Matrix4::identity());
	testClassConversions(Quat(1.0, 0.0f, 0.0f, 2.0f));

	ASSERT(fromString<vector<int>>("1 2 3 4 5") == vector<int>({1, 2, 3, 4, 5}));
	ASSERT(fromString<float2>("100 \r\t\n  1") == float2(100, 1));

	ASSERT(fromString<vector<float2>>("1 2 4 5.5") == vector<float2>({{1, 2}, {4, 5.5}}));

	ASSERT(toString(vector<int>({4, 5, 6, 7, 8})).text() == string("4 5 6 7 8"));
	ASSERT(toString(vector<float>({1, 2, 3, 4.5, 5.5, 6})).text() == string("1 2 3 4.5 5.5 6"));
	ASSERT_EXCEPTION(fromString<vector<int>>("1 2a 3"));
	ASSERT_EXCEPTION(fromString<bool>("foobar"));
}

void testPathOperations() {
	// TODO: write me
}

void testMain() {
	testTextFormatter();
	testXMLConverters();
	testPathOperations();
}
