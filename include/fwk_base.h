// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_BASE_H
#define FWK_BASE_H

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

#if _MSC_VER
#define NOINLINE
#define __restrict__ __restrict
#define ALWAYS_INLINE
#else
#define NOINLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline))
#endif

#ifdef __clang__
#define ATTRIB_PRINTF(fmt, next) __attribute__((__format__(__printf__, fmt, next)))
#else
#define ATTRIB_PRINTF(fmt, next)
#endif

#define FWK_MOVABLE_CLASS(Class)                                                                   \
	~Class();                                                                                      \
	Class(const Class &) = delete;                                                                 \
	Class(Class &&);                                                                               \
	Class &operator=(const Class &) = delete;                                                      \
	Class &operator=(Class &&);

#define FWK_MOVABLE_CLASS_IMPL(Class)                                                              \
	Class::~Class() = default;                                                                     \
	Class::Class(Class &&) = default;                                                              \
	Class &Class::operator=(Class &&) = default;

#define FWK_COPYABLE_CLASS(Class)                                                                  \
	~Class();                                                                                      \
	Class(const Class &);                                                                          \
	Class(Class &&);                                                                               \
	Class &operator=(const Class &);                                                               \
	Class &operator=(Class &&);

#define FWK_COPYABLE_CLASS_IMPL(Class)                                                             \
	FWK_MOVABLE_CLASS_IMPL(Class)                                                                  \
	Class::Class(const Class &) = default;                                                         \
	Class &Class::operator=(const Class &) = default;

#include "fwk/sys/memory.h"

namespace fwk {

using std::array;
using std::move;
using std::pair;
using std::string;
using std::swap;
using string32 = std::u32string;
using std::shared_ptr;
using std::unique_ptr;

using std::begin;
using std::end;
using std::make_pair;
using std::make_shared;
using std::make_unique;

// TODO: use types from cstdint
using uint = unsigned int;
using u8 = unsigned char;
using i8 = char;
using u16 = unsigned short;
using i16 = short;
using u32 = unsigned;
using i32 = int;
using u64 = unsigned long long;
using i64 = long long;

struct Empty {};

template <class... T> struct Undefined;
template <long long... V> struct UndefinedVal;
template <class T> using UndefinedSize = UndefinedVal<sizeof(T)>;

template <class T, int size> constexpr int arraySize(T (&)[size]) { return size; }

template <class T, class T1, class T2>
constexpr bool inRange(const T &value, const T1 &begin, const T2 &end) {
	return value >= begin && value < end;
}

template <class T1, class T2> constexpr bool isSame() { return std::is_same<T1, T2>::value; }
template <class T> constexpr bool isConst() { return std::is_const<T>::value; }

template <bool value, class T1, class T2>
using Conditional = typename std::conditional<value, T1, T2>::type;
template <class T> using RemoveConst = typename std::remove_const<T>::type;
template <class T> using RemoveReference = typename std::remove_reference<T>::type;

template <typename... Types> class Variant;

template <class T1, class T2> bool operator!=(const T1 &a, const T2 &b) { return !(a == b); }

template <class T, class... Args>
constexpr const T &max(const T &arg1, const T &arg2, const Args &... args) {
	if constexpr(sizeof...(Args) > 0)
		return max(max(arg1, arg2), args...);
	return (arg2 < arg1) ? arg1 : arg2;
}

template <class T, class... Args>
constexpr const T &min(const T &arg1, const T &arg2, const Args &... args) {
	if constexpr(sizeof...(Args) > 0)
		return min(min(arg1, arg2), args...);
	return (arg1 < arg2) ? arg1 : arg2;
}

struct EnabledType {};
struct DisabledType;
struct IsNotTied;

namespace detail {

	struct NoAssertsTag {};
	struct ValidType {
		template <class A> using Arg = A;
	};

	template <class T> struct IsRefTuple {
		enum { value = 0 };
		using type = std::false_type;
	};

	template <class... Members> struct IsRefTuple<std::tuple<Members &...>> {
		enum { value = 1 };
		using type = std::true_type;
	};

	template <class T> struct HasTiedFunction {
		template <class C>
		static auto test(int) -> typename IsRefTuple<decltype(((C *)nullptr)->tied())>::type;
		template <class C> static auto test(...) -> std::false_type;
		enum { value = std::is_same<std::true_type, decltype(test<T>(0))>::value };
	};

	template <class...> struct Conjunction : std::true_type {};
	template <class B1> struct Conjunction<B1> : B1 {};
	template <class B1, class... Bn>
	struct Conjunction<B1, Bn...> : std::conditional_t<bool(B1::value), Conjunction<Bn...>, B1> {};
}

template <bool cond, class InvalidArg = DisabledType>
using EnableIf =
	typename std::conditional<cond, detail::ValidType, InvalidArg>::type::template Arg<EnabledType>;

template <class T> constexpr bool isTied() { return detail::HasTiedFunction<T>::value; }
template <class T> using EnableIfTied = EnableIf<detail::HasTiedFunction<T>::value, IsNotTied>;

#define FWK_TIE_MEMBERS(...)                                                                       \
	auto tied() const { return std::tie(__VA_ARGS__); }
#define FWK_ORDER_BY(name, ...)                                                                    \
	FWK_TIE_MEMBERS(__VA_ARGS__)                                                                   \
	bool operator==(const name &rhs) const { return tied() == rhs.tied(); }                        \
	bool operator<(const name &rhs) const { return tied() < rhs.tied(); }

template <class T> class immutable_ptr;
template <class T> class immutable_weak_ptr;

template <class T, int size, int minimum_size> constexpr inline void validateSize() {
	static_assert(size >= minimum_size, "Invalid size");
}

// T definition is not required, you just have to make sure
// that size is at least as big as sizeof(T)
// It's similar to unique_ptr but it keeps the data in-place
// TODO: better name ?
template <class T, unsigned size> struct StaticPimpl {
	template <class... Args> StaticPimpl(Args &&... args) {
		validateSize<T, size, sizeof(T)>();
		new(data) T(std::forward<Args>(args)...);
	}
	StaticPimpl(const StaticPimpl &rhs) { new(data) T(*rhs); }
	StaticPimpl(StaticPimpl &&rhs) { new(data) T(move(*rhs)); }
	~StaticPimpl() { (**this).~T(); }

	void operator=(const StaticPimpl &rhs) { **this = *rhs; }
	void operator=(StaticPimpl &&rhs) { **this = move(*rhs); }

	const T &operator*() const {
		validateSize<T, size, sizeof(T)>();
		return reinterpret_cast<const T &>(*this);
	}
	T &operator*() {
		validateSize<T, size, sizeof(T)>();
		return reinterpret_cast<T &>(*this);
	}
	T *operator->() { return &operator*(); }
	const T *operator->() const { return &operator*(); }

  private:
	char data[size];
};

#define FWK_STRINGIZE(...) FWK_STRINGIZE_(__VA_ARGS__)
#define FWK_STRINGIZE_(...) #__VA_ARGS__

// TODO: move FATAL, check, etc to fwk_assert ?
// TODO: use CString -> CString
[[noreturn]] void fatalError(const char *file, int line, const char *fmt, ...) ATTRIB_PRINTF(3, 4);
[[noreturn]] void assertFailed(const char *file, int line, const char *str);
[[noreturn]] void checkFailed(const char *file, int line, const char *fmt, ...) ATTRIB_PRINTF(3, 4);

void handleCtrlC(void (*handler)());
void handleSegFault();

void sleep(double sec);
double getTime();

// TODO: maybe FILE / LINE is not required if we have backtraces?

#define FATAL(...) fwk::fatalError(__FILE__, __LINE__, __VA_ARGS__)

#define ASSERT(expr) ((!!(expr) || (fwk::assertFailed(__FILE__, __LINE__, FWK_STRINGIZE(expr)), 0)))

// TODO: Error messages using fwk format ?
#define CHECK_FAILED(...) fwk::checkFailed(__FILE__, __LINE__, __VA_ARGS__)
// Use this for checking input; If rollback mode is on, it will cause rollback()
// Otherwise it works just like an assert
#define CHECK(expr)                                                                                \
	(!!(expr) || (fwk::checkFailed(__FILE__, __LINE__, "%s", FWK_STRINGIZE(expr)), 0))

#ifdef NDEBUG
#define DASSERT(expr) ((void)0)
#else
#define DASSERT(expr) ASSERT(expr)
#endif

#if defined(FWK_PARANOID) && !defined(NDEBUG)
#define PASSERT(expr) ASSERT(expr)
#else
#define PASSERT(expr) ((void)0)
#endif
}

#include "fwk_maybe.h"
#include "fwk_range.h"
#include "fwk_vector.h"

#include "fwk/enum.h"
#include "fwk_index_range.h"

namespace fwk {

class Backtrace;

void logError(const string &error);

#define SAFE_ARRAY(declaration, size, ...)                                                         \
	declaration[] = {__VA_ARGS__};                                                                 \
	static_assert(COUNT_ARGUMENTS(__VA_ARGS__) == size, "Invalid number of elements in an array");

class Stream;
template <class T> struct SerializeAsPod;

// TODO: use std::is_trivially_copyable instead?
#define SERIALIZE_AS_POD(type)                                                                     \
	namespace fwk {                                                                                \
		template <> struct SerializeAsPod<type> {                                                  \
			enum { value = 1 };                                                                    \
		};                                                                                         \
	}

pair<string, bool> execCommand(const string &cmd);

class CString;
class FilePath;

class XMLNode;
class XMLDocument;

class TextFormatter;
class TextParser;
}

#endif
