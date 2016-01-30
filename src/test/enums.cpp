#include "testing.h"

DEFINE_ENUM(SomeEnum, foo, bar, foo_bar, last);

void testMain() {
	ASSERT(fromString<SomeEnum>("foo") == SomeEnum::foo);
	ASSERT_EXCEPTION(fromString<SomeEnum>("something else"));
	ASSERT(tryFromString<SomeEnum>("something else") == none);
	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));

	SAFE_ARRAY(int array, count<SomeEnum>(), 1, 2, 3, 4);

	ASSERT(array[SomeEnum::foo_bar] == 3);
	string text;
	for(auto elem : all<SomeEnum>())
		text += toString(elem);
	ASSERT(text == "foobarfoo_barlast");
}
