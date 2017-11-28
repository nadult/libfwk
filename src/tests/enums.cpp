// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/enum_map.h"
#include "fwk/sys/assert.h"
#include "testing.h"

DEFINE_ENUM(SomeEnum, foo, bar, foo_bar, last);

struct Temp {
	struct Inside {
		DEFINE_ENUM_MEMBER(MemberEnum, aaa, bbb, ccc, ddd);
	};
};

void testMain() {
	ASSERT(bool() == false);
	ASSERT_EQ(fromString<SomeEnum>("foo"), SomeEnum::foo);
	ASSERT_FAIL(fromString<SomeEnum>("something else"));
	ASSERT(!tryFromString<SomeEnum>("something else"));
	ASSERT_EQ(string("foo_bar"), toString(SomeEnum::foo_bar));
	ASSERT_EQ(toString(Temp::Inside::MemberEnum::ccc), string("ccc"));

	EnumMap<SomeEnum, int> array{{1, 2, 3, 4}};

	static_assert(!isEnum<int>(), "");
	static_assert(isEnum<SomeEnum>(), "");

	ASSERT(array[SomeEnum::foo_bar] == 3);
	string text;
	for(auto elem : all<SomeEnum>())
		text += toString(elem);
	ASSERT_EQ(text, "foobarfoo_barlast");
	ASSERT(mask(false, SomeEnum::foo) == none);
	ASSERT(mask(true, SomeEnum::bar) == SomeEnum::bar);
}
