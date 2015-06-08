#include "fwk_base.h"

using namespace fwk;

DECLARE_ENUM(SomeEnum, foo, bar, foo_bar);

DEFINE_ENUM(SomeEnum, "foo", "bar", "foo_bar");

#define ASSERT_EXCEPTION(code)                                                                     \
	{                                                                                              \
		bool exception_thrown = false;                                                             \
		try {                                                                                      \
			code ;                                                                                  \
		} catch(...) { exception_thrown = true; }                                                  \
		ASSERT(exception_thrown);                                                                  \
	}

int main() {
	ASSERT(SomeEnum::fromString("foo") == SomeEnum::foo);
	ASSERT_EXCEPTION( SomeEnum::fromString("something else") );
	ASSERT(SomeEnum::fromString("something else", true) == SomeEnum::invalid);

	ASSERT(string("foo_bar") == toString(SomeEnum::foo_bar));
	ASSERT(string("not valid") == toString(SomeEnum::invalid, "not valid"));
	ASSERT(toString(SomeEnum::invalid) == nullptr);

	printf("All OK\n");
	return 0;
}
