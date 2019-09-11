// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/any.h"
#include "fwk/fwd_member.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/sys/file_system.h"
#include "fwk/sys/on_fail.h"
#include "fwk/tag_id.h"
#include "fwk/type_info_gen.h"
#include "fwk/variant.h"
#include "testing.h"

DEFINE_ENUM(SomeTag, foo, bar);

void testTypes() {
	static_assert(is_same<SubtractTypes<Types<int, float, char, llint>, Types<int, llint>>,
						  Types<float, char>>);
	static_assert(type_index<float, Types<int, float, float, char>> == 1);
	static_assert(unique_types<float, char, int>);
	static_assert(!unique_types<float, char, int, float>);

	static_assert(!is_convertible<TagId<SomeTag::foo>, TagId<SomeTag::bar>>);
}

void testTextFormatter() NOEXCEPT {
	TextFormatter fmt;
	fmt.stdFormat("%d %x %s", 11, 0x20, "foobar");
	ASSERT(fmt.text() == string("11 20 foobar"));

	bool array_of_bools[4] = {false, true, false, true};
	ASSERT_EQ(toString(array_of_bools), "false true false true");

	string array_of_strings1[3] = {"heeloo", "yallala", "foobar!"};
	ASSERT_EQ(toString(array_of_strings1), "heeloo yallala foobar!");

	Str array_of_strings2[3] = {"heeloo", "yallala", "foobar!"};
	ASSERT_EQ(toString(array_of_strings2), "heeloo yallala foobar!");

	Pair<int, double> some_pair = {10, 12.5};
	ASSERT_EQ(toString(some_pair), "10 12.5");

	ASSERT_EQ(format("\\%%\\%%\\%", "foo", "bar"), "%foo%bar%");
	ASSERT(!exceptionRaised());
}

template <class T> void testClassConversions(T value) NOEXCEPT {
	string str = toString(value);
	ASSERT_EQ(fromString<T>(str.c_str()), value);
	ASSERT(!exceptionRaised());

	vector<T> vec = {value, value, value, value};
	string vec_str = toString(vec);
	ASSERT_EQ(fromString<vector<T>>(vec_str.c_str()), vec);
	ASSERT(!exceptionRaised());
}

void testString() {
	ASSERT_EQ(Str("random text").limitSizeFront(8), "... text");
	ASSERT_EQ(Str("random text").limitSizeBack(8), "rando...");
	ASSERT_EQ(Str("foo bar").find("bar"), 4);
	ASSERT_EQ(Str("foo | bar").find('|'), 4);
	ASSERT_EQ(string("foo"), Str("foo"));
	ASSERT(Str("foo") < Str("foobar"));
	ASSERT(Str("foobar").compareIgnoreCase("Foo") == 1);
}

void testVariant() {
	using Var1 = Variant<string, FBox>;
	Var1 var = string("woohoo");
	XmlDocument doc;
	auto node = doc.addChild("node");
	save(node, var);

	static_assert(is_xml_loadable<Var1>);
	static_assert(is_xml_saveable<Var1>);

	auto temp = load<Var1>(node);
	ASSERT(temp == var);
	ASSERT_EQ(toString(temp.get()), "woohoo");

	using Var2 = Variant<int, float>;
	auto temp2 = load<Var2>(node);
	ASSERT(!temp2);
}

void testAny() {
	XmlDocument doc;
	auto node = doc.addChild("test");
	Any any1(1234);
	any1.save(node);
	Any(false).save(doc.addChild("bool_node"));

	auto any2 = Any::load(node).get();
	ASSERT_EQ(any2.get<int>(), 1234);

	auto any3 = Any::load(node, "bool");
	ASSERT(!any3);
}

void testXMLConverters() NOEXCEPT {
	ASSERT(TextParser("1 2 aa bb cc 4d").countElements() == 6);

	testClassConversions(99);
	testClassConversions(1234.5f);
	testClassConversions(568u);
	testClassConversions(string("foobar"));

	testClassConversions(int2(4, 5));
	testClassConversions(int3(6, 7, 99));
	testClassConversions(int4(10, 11, 1000, 11));
	testClassConversions(123.456f);
	testClassConversions(12345.6789);
	testClassConversions(float2(11, 17));
	testClassConversions(float3(45, 67, 1.5));
	testClassConversions(float4(1, 1.5, 5.5, 12.5));

	testClassConversions(IRect({1, 2}, {3, 4}));
	testClassConversions(FRect({0, 0.5}, {2, 2.5}));
	testClassConversions(IBox({0, 0, 0}, {30, 30, 30}));
	testClassConversions(FBox({10, 20, 30}, {100, 200, 300}));
	testClassConversions(Matrix4::identity());
	testClassConversions(Quat(1.0, 0.0f, 0.0f, 2.0f));

	static_assert(is_parsable<vector<DBox>>);
	static_assert(is_parsable<vector<DRect>>);
	static_assert(!is_parsable<vector<vector<int>>>);

	struct MyClass {
		double x = 16.128;
		operator double() const { return x; }
	};

	ASSERT_EQ(toString(MyClass()), "16.128");
	ASSERT_EQ(fromString<vector<int>>("1 2 3 4 5"), vector<int>({1, 2, 3, 4, 5}));
	ASSERT_EQ(fromString<float2>("100 \r\t\n  1"), float2(100, 1));

	ASSERT_EQ(fromString<vector<float2>>("1 2 4 5.5"), vector<float2>({{1, 2}, {4, 5.5}}));
	ASSERT_EQ(toString(vector<int>().size()), "0");
	ASSERT(!exceptionRaised());

	ASSERT_EQ(toString(vector<int>({4, 5, 6, 7, 8})), "4 5 6 7 8");
	ASSERT_EQ(toString(vector<float>({1, 2, 3, 4.5, 5.5, 6})), "1 2 3 4.5 5.5 6");
	ASSERT_EQ(toString("foo"), "foo");
	ASSERT_EQ(toString(short(10)), "10");

	ASSERT(!maybeFromString<vector<int>>("1 2a 3"));
	ASSERT(!maybeFromString<bool>("foobar"));
	ASSERT(!maybeFromString<int>("10000000000"));
	ASSERT(!maybeFromString<short>("32768"));
	ASSERT(!maybeFromString<unsigned short>("-1"));
	ASSERT_EQ(fromString<long long>("1000000000000"), 1000000000000ll);
	ASSERT(!exceptionRaised());

	ASSERT_EQ(transform<int>(intRange(5)), (vector<int>{{{0, 1, 2, 3, 4}}}));
	ASSERT_EQ(transform<Pair<int>>(pairsRange(3)), (vector<Pair<int>>{{{0, 1}, {1, 2}}}));

	auto even_filter = [](int v) { return v % 2 == 0; };
	IndexRange even_ints(0, 10, none, even_filter);
	ASSERT_EQ(transform<int>(even_ints), (vector<int>{{{0, 2, 4, 6, 8}}}));
}

void testPathOperations() {
#ifdef FWK_TARGET_LINUX
	ASSERT(!mkdirRecursive("/totally_crazy_path/no_way_its_possible"));
#endif
	// TODO: write me
}

void testFunc1(Span<int, 5>) {}
void testFunc2(CSpan<int, 5>) {}

void testRanges() {
	array<int, 5> tab1;
	testFunc1(tab1);
	testFunc2(tab1);
	const array<int, 5> tab2 = {{1, 2, 3, 4, 5}};
	testFunc2(tab2);
	// TODO: write me
}

void testMaybe() {
	Maybe<IRect> mrect;
	mrect = IRect(0, 0, 10, 10).intersection(IRect(20, 20, 30, 30));
	ASSERT(!mrect);
	ASSERT(IRect(0, 0, 10, 10).intersection(IRect(1, 1, 20, 20)));

	ASSERT(Maybe<int>(10) == 10);
	ASSERT(Maybe<int>() < 10);

	static_assert(sizeof(Maybe<IRect>) == sizeof(IRect));
	static_assert(sizeof(Maybe<FormatMode>) == 1);

	// TODO: write more
}

void testTypeInfo() {
	ASSERT_EQ(typeInfo<float const &>().name(), string("float const&"));
	ASSERT_EQ(typeInfo<int volatile const &>().name(), string("int const volatile&"));
	ASSERT_EQ(typeInfo<vector<const int *> const &>().name(),
			  string("fwk::Vector<int const*> const&"));
	ASSERT_EQ(typeInfo<int const *const *&>().referenceBase()->pointerBase()->name(),
			  string("int const* const"));
	ASSERT_EQ(typeInfo<double *volatile>().asConst().name(), string("double* const volatile"));
	ASSERT_EQ(typeInfo<double &>().asConst().name(), string("double&"));
}

void testFwdMember() {
	struct Undef;
	// TODO: why this stopped working?
	//static_assert(!fwk::detail::FullyDefined<Vector<Vector<Vector<Dynamic<Undef>>>>>::value);
	static_assert(fwk::detail::FullyDefined<Pair<Vector<int>, Vector<int>>>::value);
}

void testExceptions() NOEXCEPT {
	string test_string = "value0";
	RAISE("Invalid string: %", test_string);
	auto exception_text = toString(getMergedExceptions());
	ASSERT(Str(exception_text).contains(test_string));

	CHECK(test_string == "nope", test_string);
	exception_text = toString(getMergedExceptions());
	ASSERT(Str(exception_text).contains(test_string));
}

void testVector() {
	vector<int> vec = {10, 20, 40, 50};

	vector<vector<int>> vvals;
	for(int k = 0; k < 4; k++)
		vvals.emplace_back(vec);
	vvals.erase(vvals.begin() + 1);
	vvals.erase(vvals.begin() + 2);
	ASSERT(toString(vvals) == "10 20 40 50 10 20 40 50");

	vec.insert(vec.begin() + 2, 30);
	vec.erase(vec.begin() + 3, vec.end());
	auto copy = vec;
	ASSERT(toString(copy) == "10 20 30");

	vector<string> vecs = {"xxx", "yyy", "zzz", "xxx", "abc", "abc", "zzz"};
	ASSERT(toString(sortedUnique(vecs)) == "abc xxx yyy zzz");
}

void testMain() {
	testString();
	testAny();
	testTextFormatter();
	testXMLConverters();
	testPathOperations();
	testMaybe();
	testTypeInfo();
	testFwdMember();
	testVariant();
	testTypes();
	testExceptions();
	testVector();
}
