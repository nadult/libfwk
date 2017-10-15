// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <cstdlib>

#include <iostream>
#include <string>

#include <boost/variant.hpp>

#include "fwk/format.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk_variant.h"
#include "timer.h"

using fwk::TestTimer;

class SizeVisitor : public boost::static_visitor<> {
  public:
	int size = 0;
	template <class T> void operator()(const T &rhs) { size = sizeof(rhs); }
};

template <class Visitor, typename... Types>
auto visit(boost::variant<Types...> &variant, Visitor &visitor) {
	boost::apply_visitor(visitor, variant);
}

template <class Visitor, typename... Types>
auto visit(fwk::Variant<Types...> &variant, Visitor &visitor) {
	fwk::apply_visitor(visitor, variant);
}

DEFINE_ENUM(enum1, aa, bb, cc);
DEFINE_ENUM(enum2, qq, rr, tt);

template <template <typename...> class T> void testVariantSimple(const char *name) {
	TestTimer t(name);
	using namespace fwk;

	using Type = T<enum1, enum2, short>;
	int iters = 10000000;

	vector<Type> values;
	values.reserve(iters);

	srand(0);
	for(int n = 0; n < iters; n++) {
		Type variant;
		int t = rand();
		if(t % 3 == 0)
			variant = (enum1)(t / 128);
		else if(t % 3 == 1) {
			variant = (enum2)(t / 128);
		} else
			variant = (short)(t / 64);
		values.emplace_back(variant);
	}

	int sum = 0;
	for(auto &v : values) {
		SizeVisitor visitor;
		visit(v, visitor);
		sum += visitor.size;
	}

	print("Result: % Size: %\n", sum, sizeof(Type));
}

template <template <typename...> class T> void testVariantBigger(const char *name) {
	TestTimer t(name);
	using namespace fwk;

	using Type = T<Matrix4, IRect, string>;
	int iters = 3000000;

	vector<Type> values;
	values.reserve(iters);

	srand(0);
	for(int n = 0; n < iters; n++) {
		Type variant;
		int t = rand();
		if(t % 3 == 0)
			variant = Matrix4::identity();
		else if(t % 3 == 1) {
			variant = IRect({10, 10}, {20, 20});
		} else
			variant = "Hello world";
		values.emplace_back(variant);
	}

	int sum = 0;
	for(auto &v : values) {
		SizeVisitor visitor;
		visit(v, visitor);
		sum += visitor.size;
	}

	print("Result: % Size: %\n", sum, sizeof(Type));
}

int main() {
	testVariantSimple<boost::variant>("boost::variant simple");
	testVariantSimple<fwk::Variant>("fwk::Variant   simple");
	testVariantBigger<boost::variant>("boost::variant bigger");
	testVariantBigger<fwk::Variant>("fwk::Variant   bigger");

	return 0;
}
