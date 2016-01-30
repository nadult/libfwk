#include "testing.h"

ENUM(SomeEnum, foo, bar, foo_bar);

void testMain() {
	ASSERT(SomeEnum("foo") == SomeEnum::foo);
	ASSERT_EXCEPTION(SomeEnum("something else"));
	ASSERT(fromString<SomeEnum>("something else") == none);
	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));

	ENUM_SIMPLE(LocalEnum, first_element, middle_element, last_element);
	int array[LocalEnum::count] = {1, 2, 3};

	ASSERT(array[LocalEnum::middle_element] == 2);
	ASSERT(LocalEnum::last_element > LocalEnum::middle_element);
}
