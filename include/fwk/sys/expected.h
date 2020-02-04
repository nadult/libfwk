// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/maybe.h"
#include "fwk/sys/exception.h"

namespace fwk {

// Will return ERROR if there are any exceptions raised or if expr evaluates to false.
// If expr is of Expected<> type, then it's error will be passed forward.
#define EXPECT(expr)                                                                               \
	{                                                                                              \
		if(fwk::exceptionRaised())                                                                 \
			return fwk::getMergedExceptions();                                                     \
		auto &&_expect_value = ((expr));                                                           \
		if(__builtin_expect(!_expect_value, false))                                                \
			return fwk::detail::passError(_expect_value, FWK_STRINGIZE(expr), __FILE__, __LINE__); \
	}

// Checks if there are any raised exceptions. If so, it will return them.
// You can assume that this construction is excecuted whenever Expected<> is
// constructed/moved/copied or when EXPECT(...) check is evaluated.
// EX_CATCH should be used in places where lack of exception is necessary for proper execution.
#define EX_CATCH()                                                                                 \
	{                                                                                              \
		if(fwk::exceptionRaised())                                                                 \
			return fwk::getMergedExceptions();                                                     \
	}

// Evaluates an expression of type Expected<T>.
// If it's valid then the value is simply passed, otherwise error is returned.
//
// Example use:
// Ex<int> func1(int v) { ... }
// Ex<float> func2() { auto value = EX_PASS(func1(10)); return value * 0.5f; }
#define EX_PASS(...)                                                                               \
	({                                                                                             \
		auto _expect_result = __VA_ARGS__;                                                         \
		static_assert(fwk::is_expected<decltype(_expect_result)>,                                  \
					  "You have to pass Expected<...> to EX_PASS");                                \
		if(!_expect_result)                                                                        \
			return _expect_result.error();                                                         \
		move(_expect_result.get());                                                                \
	})

namespace detail {
	template <class T> struct RemoveExpected { using Type = T; };
	template <class T> struct RemoveExpected<Expected<T>> { using Type = T; };

	// Implemented in on_fail.cpp
	Error expectMakeError(const char *, const char *, int);
	void expectFromExceptions(Dynamic<Error> *);
	void expectMergeExceptions(Error &);

}

template <class T> constexpr bool is_expected = false;
template <class T> constexpr bool is_expected<Expected<T>> = true;

template <class T> using RemoveExpected = typename detail::RemoveExpected<T>::Type;

// Simple class which can hold value or an error.
// When constructed checks if there are any exceptions raised and if so, retrieves them.
template <class T> class NOEXCEPT [[nodiscard]] Expected {
  public:
	static_assert(!is_same<T, Error>);

	// TODO: should we check here if there were any errors? and return them if so?
	Expected(const T &value) : m_has_value(!exceptionRaised()) {
		if(exceptionRaised())
			detail::expectFromExceptions(&m_error);
		else
			new(&m_value) T(value);
	}
	Expected(T && value) : m_has_value(!exceptionRaised()) {
		if(exceptionRaised())
			detail::expectFromExceptions(&m_error);
		else
			new(&m_value) T(move(value));
	}
	Expected(Error error) : m_error(move(error)), m_has_value(false) {
		if(exceptionRaised())
			detail::expectMergeExceptions(*m_error);
	}
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

	void check() const {
		if(!m_has_value)
			fatalError(*m_error);
	}

	T &get() {
		if(!m_has_value)
			fatalError(*m_error);
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
	Expected() {
		if(exceptionRaised())
			detail::expectFromExceptions(&m_error);
	}
	Expected(Error error) : m_error(move(error)) {
		if(exceptionRaised())
			detail::expectMergeExceptions(*m_error);
	}

	void swap(Expected & rhs) { fwk::swap(m_error, rhs.m_error); }

	Error &error() { return DASSERT(m_error.get()), *m_error; }
	const Error &error() const { return DASSERT(m_error.get()), *m_error; }

	explicit operator bool() const { return !m_error.get(); }

	void check() const {
		if(m_error.get())
			fatalError(*m_error);
	}
	void ignore() const {}

	void get() const { check(); }

  private:
	Dynamic<Error> m_error;
};

namespace detail {

	template <class T>
	auto passError(const T &value, const char *expr, const char *file, int line) {
		return expectMakeError(expr, file, line);
	}

	template <class T> auto passError(const Expected<T> &value, const char *, const char *, int) {
		return value.error();
	}

	template <class T, class... Args> struct IsExConstructible {
		FWK_SFINAE_TYPE(Type, T, DECLVAL(U).exConstruct(DECLVAL(Args)...));
		static constexpr bool value = is_same<Type, Ex<void>>;
	};
}

template <class T, class... Args>
constexpr bool is_ex_constructible =
	detail::IsExConstructible<T, Args...>::value &&std::is_default_constructible<T>::value;

// Convenient function for classes which have special kind of constructor:
// member function which initializes given class and returns Ex<void>;
//
// Example use:
// class MyClass {
//     Ex<void> exConstruct(int argument) {
//       EXPECT(argument > 0);
//       foo = argument;
//       return {};
//     }
//     int foo = 0;
// };
// ...
// construct<MyClass>(11);
template <class T, class... Args, EnableIf<is_ex_constructible<T, Args...>>...>
Ex<T> construct(Args &&... args) {
	T out;
	auto result = out.exConstruct(std::forward<Args>(args)...);
	if(!result)
		return result.error();
	return out;
}

}
