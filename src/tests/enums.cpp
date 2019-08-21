// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "testing.h"

DEFINE_ENUM(SomeEnum, foo, bar, foo_bar, last);
DEFINE_ENUM(BigEnum, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);

struct Temp {
	struct Inside {
		DEFINE_ENUM_MEMBER(MemberEnum, aaa, bbb, ccc, ddd);
	};
};

void testStringConversions() NOEXCEPT {
	ASSERT_EQ(maybeFromString<SomeEnum>("foo"), SomeEnum::foo);

	fromString<SomeEnum>("something else");
	ASSERT(exceptionRaised());
	clearExceptions();

	ASSERT(!maybeFromString<SomeEnum>("something else"));
	ASSERT_EQ(string("foo_bar"), toString(SomeEnum::foo_bar));
	ASSERT_EQ(toString(Temp::Inside::MemberEnum::ccc), string("ccc"));

	ASSERT_EQ(toString(SomeEnum::foo | SomeEnum::bar | SomeEnum::foo_bar), "foo|bar|foo_bar");
	ASSERT_EQ(fromString<EnumFlags<SomeEnum>>("bar|foo"), SomeEnum::bar | SomeEnum::foo);

	string text;
	for(auto elem : all<SomeEnum>)
		text += toString(elem);
	ASSERT_EQ(text, "foobarfoo_barlast");
	ASSERT(!exceptionRaised());
}

void testMain() {
	ASSERT(bool() == false);

	testStringConversions();
	EnumMap<SomeEnum, int> array{{1, 2, 3, 4}};

	static_assert(!is_enum<int>);
	static_assert(is_enum<SomeEnum>);

	ASSERT(array[SomeEnum::foo_bar] == 3);
	ASSERT(mask(false, SomeEnum::foo) == none);
	ASSERT_EQ(mask(true, SomeEnum::bar), SomeEnum::bar);

	static_assert(is_formattible<EnumFlags<SomeEnum>>);

	vector<BigEnum> items = {{BigEnum::f1, BigEnum::f2, BigEnum::f4, BigEnum::f10, BigEnum::f13}};
	EnumFlags<BigEnum> flags;
	for(auto item : items)
		flags |= item;
	ASSERT_EQ(transform<BigEnum>(flags), items);
	ASSERT_EQ(transform<BigEnum>(~flags).size(), count<BigEnum>() - items.size());
	ASSERT_EQ(countBits(EnumFlags<BigEnum>(all<BigEnum>)), count<BigEnum>());
}
