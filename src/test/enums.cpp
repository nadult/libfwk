// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/enum_map.h"
#include "testing.h"

DEFINE_ENUM(SomeEnum, foo, bar, foo_bar, last);

void testMain() {
	ASSERT(bool() == false);
	ASSERT(fromString<SomeEnum>("foo") == SomeEnum::foo);
	ASSERT_FAIL(fromString<SomeEnum>("something else"));
	ASSERT(!tryFromString<SomeEnum>("something else"));
	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));

	EnumMap<SomeEnum, int> array{{1, 2, 3, 4}};

	static_assert(!isEnum<int>(), "");
	static_assert(isEnum<SomeEnum>(), "");

	ASSERT(array[SomeEnum::foo_bar] == 3);
	string text;
	for(auto elem : all<SomeEnum>())
		text += toString(elem);
	ASSERT(text == "foobarfoo_barlast");
}
