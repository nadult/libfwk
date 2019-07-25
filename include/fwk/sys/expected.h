// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/maybe.h"
#include "fwk/sys/exception.h"

namespace fwk {

// Will return ERROR if expr evaluates to false.
// If expr is of Expected<> type, then it's error will be passed forward.
#define EXPECT(expr)                                                                               \
	{                                                                                              \
		auto &&value = ((expr));                                                                   \
		if(__builtin_expect(!value, false))                                                        \
			return fwk::detail::passError(value, FWK_STRINGIZE(expr), __FILE__, __LINE__);         \
	}

// Checks if there are any raised exceptions. If so, it will return them.
#define EXPECT_CATCH()                                                                             \
	{                                                                                              \
		if(fwk::anyExceptions())                                                                   \
			return fwk::getMergedExceptions();                                                     \
	}

// Evaluates an expression of type Expected<T>.
// If it's valid then the value is simply passed, otherwise error is returned.
//
// Example use:
// Ex<int> func1(int v) { ... }
// Ex<float> func2() { auto value = EXPECT_PASS(func1(10)); return value * 0.5f; }
#define EXPECT_PASS(...)                                                                           \
	({                                                                                             \
		auto result = __VA_ARGS__;                                                                 \
		static_assert(fwk::is_expected<decltype(result)>,                                          \
					  "You have to pass Expected<...> to EXPECT_PASS");                            \
		if(!result)                                                                                \
			return result.error();                                                                 \
		move(result.get());                                                                        \
	})

template <class T> constexpr bool is_expected = false;
template <class T> constexpr bool is_expected<Expected<T>> = true;

namespace detail {
	template <class T> struct RemoveExpected { using Type = T; };
	template <class T> struct RemoveExpected<Expected<T>> { using Type = T; };
}
template <class T> using RemoveExpected = typename detail::RemoveExpected<T>::Type;

template <class T> class NOEXCEPT [[nodiscard]] Expected {
  public:
	static_assert(!is_same<T, Error>);

	// TODO: should we check here if there were any errors? and return them if so?
	Expected(const T &value) : m_value(value), m_has_value(true) { PASSERT(!anyExceptions()); }
	Expected(T && value) : m_value(move(value)), m_has_value(true) { PASSERT(!anyExceptions()); }
	Expected(Error error) : m_error(move(error)), m_has_value(false) { PASSERT(!anyExceptions()); }
	~Expected() {
		if(m_has_value)
			m_value.~T();
		else
			m_error.~Dynamic<Error>();
	}

	Expected(const Expected &rhs) : m_has_value(rhs.m_has_value) {
		if(m_has_value)
			new(&m_value) T(rhs.m_value);
		else
			new(&m_error) Dynamic<Error>(rhs.m_error);
	}
	Expected(Expected && rhs) : m_has_value(rhs.m_has_value) {
		if(m_has_value)
			new(&m_value) T(move(rhs.m_value));
		else
			new(&m_error) Dynamic<Error>(move(rhs.m_error));
	}

	void swap(Expected & rhs) {
		Expected temp = move(*this);
		*this = move(rhs);
		rhs = move(temp);
	}

	void operator=(Expected &&rhs) {
		if(&rhs != this) {
			this->~Expected();
			new(this) Expected(move(rhs));
		}
	}

	void operator=(const Expected &rhs) {
		Expected copy(rhs);
		swap(copy);
	}

	bool hasValue() const { return m_has_value; }
	explicit operator bool() const { return m_has_value; }

	// TODO: should these call get() ? only in debug mode ?
	T *operator->() { return DASSERT(m_has_value), &m_value; }
	const T *operator->() const { return DASSERT(m_has_value), &m_value; }
	T &operator*() { return DASSERT(m_has_value), m_value; }
	const T &operator*() const { return DASSERT(m_has_value), m_value; }

	Error &error() { return DASSERT(!m_has_value), *m_error; }
	const Error &error() const { return DASSERT(!m_has_value), *m_error; }

	const T &orElse(const T &on_error) const { return m_has_value ? m_value : on_error; }

	T &get() {
		// TODO: change to FATAL ?
		if(!m_has_value)
			failedExpected(__FILE__, __LINE__, *m_error);
		return m_value;
	}
	const T &get() const { return ((Expected *)this)->get(); }
	operator Maybe<T>() const { return m_has_value ? m_value : Maybe<T>(); }

	bool operator==(const T &rhs) const { return m_has_value && m_value == rhs; }
	friend bool operator==(const T &a, const Expected<T> &b) {
		return b.m_has_value && b.m_value == a;
	}

  private:
	union {
		Dynamic<Error> m_error;
		T m_value;
	};
	bool m_has_value;
};

template <> class [[nodiscard]] Expected<void> {
  public:
	// TODO: should we check here if there were any errors? and return them if so?
	Expected() { PASSERT(!anyExceptions()); }
	Expected(Error error) : m_error(move(error)) { PASSERT(!anyExceptions()); }

	void swap(Expected & rhs) { fwk::swap(m_error, rhs.m_error); }

	Error &error() { return DASSERT(m_error.get()), *m_error; }
	const Error &error() const { return DASSERT(m_error.get()), *m_error; }

	explicit operator bool() const { return !m_error.get(); }

	void check() const {
		if(m_error.get())
			failedExpected(__FILE__, __LINE__, *m_error);
	}
	void ignore() const {}

	void get() const { check(); }

  private:
	Dynamic<Error> m_error;
};

namespace detail {
	template <class T>
	auto passError(const T &value, const char *expr, const char *file, int line) {
		return fwk::Error({file, line}, expr);
	}

	template <class T> auto passError(const Expected<T> &value, const char *, const char *, int) {
		return value.error();
	}
}
}
