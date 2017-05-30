#ifndef FWK_ASSERT_H
#define FWK_ASSERT_H

#include "fwk_xml.h"

namespace fwk {

namespace detail {

	template <class T>
	void assertFailedBinary(const char *, int, const char *, const char *, const char *, const T &,
							const T &) NOINLINE NORETURN;

	template <class T>
	void assertFailedBinary(const char *file, int line, const char *op, const char *str1,
							const char *str2, const T &v1, const T &v2) {
		assertFailed(file, line, xmlFormat("%:% % %:%", str1, v1, op, str2, v2).c_str());
	}
}

#define ASSERT_BINARY(expr1, expr2, op)                                                            \
	(((expr1)op(expr2) || (fwk::detail::assertFailedBinary(                                        \
							   __FILE__, __LINE__, FWK_STRINGIZE(op), FWK_STRINGIZE(expr1),        \
							   FWK_STRINGIZE(expr2), (expr1), (expr2)),                            \
						   0)))

#define ASSERT_EQ(expr1, expr2) ASSERT_BINARY(expr1, expr2, ==)
#define ASSERT_NE(expr1, expr2) ASSERT_BINARY(expr1, expr2, !=)
#define ASSERT_GR(expr1, expr2) ASSERT_BINARY(expr1, expr2, >)
#define ASSERT_LT(expr1, expr2) ASSERT_BINARY(expr1, expr2, <)
#define ASSERT_LE(expr1, expr2) ASSERT_BINARY(expr1, expr2, <=)
#define ASSERT_GE(expr1, expr2) ASSERT_BINARY(expr1, expr2, >=)

#ifdef NDEBUG
#define DASSERT_EQ(expr1, expr2) ((void)0)
#define DASSERT_NE(expr1, expr2) ((void)0)
#define DASSERT_GR(expr1, expr2) ((void)0)
#define DASSERT_LT(expr1, expr2) ((void)0)
#define DASSERT_LE(expr1, expr2) ((void)0)
#define DASSERT_GE(expr1, expr2) ((void)0)
#else
#define DASSERT_EQ(expr1, expr2) ASSERT_EQ(expr1, expr2)
#define DASSERT_NE(expr1, expr2) ASSERT_NE(expr1, expr2)
#define DASSERT_GR(expr1, expr2) ASSERT_GR(expr1, expr2)
#define DASSERT_LT(expr1, expr2) ASSERT_LT(expr1, expr2)
#define DASSERT_LE(expr1, expr2) ASSERT_LE(expr1, expr2)
#define DASSERT_GE(expr1, expr2) ASSERT_GE(expr1, expr2)
#endif
}

#endif
