#include "testing.h"

DECLARE_ENUM(SomeEnum, foo, bar, foo_bar);
DEFINE_ENUM(SomeEnum, "foo", "bar", "foo_bar");

void testMain() {
	ASSERT(SomeEnum::fromString("foo") == SomeEnum::foo);
	ASSERT_EXCEPTION(SomeEnum::fromString("something else"));
	ASSERT(SomeEnum::fromString("something else", false) == SomeEnum::invalid);

	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));
	ASSERT(string("not valid") == toString(SomeEnum::invalid, "not valid"));
	ASSERT(toString(SomeEnum::invalid) == nullptr);
}
