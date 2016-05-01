#include "testing.h"

DEFINE_ENUM(SomeEnum, foo, bar, foo_bar, last);

void testMain() {
	ASSERT(bool() == false);
	ASSERT(fromString<SomeEnum>("foo") == SomeEnum::foo);
	ASSERT_EXCEPTION(fromString<SomeEnum>("something else"));
	ASSERT(!tryFromString<SomeEnum>("something else"));
	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));

	EnumMap<SomeEnum, int> array{{1, 2, 3, 4}};

	ASSERT(array[SomeEnum::foo_bar] == 3);
	string text;
	for(auto elem : all<SomeEnum>())
		text += toString(elem);
	ASSERT(text == "foobarfoo_barlast");
}
