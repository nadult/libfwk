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
// TODO: use StringRef -> CString
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

// TODO: change name to CString
// TODO: move to string_ref.cpp
// Simple reference to string
// User have to make sure that referenced data is alive as long as StringRef
class StringRef {
  public:
	StringRef(const string &str) : m_data(str.c_str()), m_length((int)str.size()) {}
	StringRef(const char *str, int length) : m_data(str ? str : ""), m_length(length) {
		PASSERT((int)strlen(str) >= length);
	}
	StringRef(const char *str) {
		if(!str)
			str = "";
		m_data = str;
		m_length = strlen(str);
	}
	// TODO: conversion from CSpan<char>? but what about null-termination
	StringRef() : m_data(""), m_length(0) {}

	operator string() const { return string(m_data, m_data + m_length); }
	const char *c_str() const { return m_data; }

	int size() const { return m_length; }
	int length() const { return m_length; }
	bool empty() const { return m_length == 0; }
	int compare(const StringRef &rhs) const;
	int caseCompare(const StringRef &rhs) const;

	const char *begin() const { return m_data; }
	const char *end() const { return m_data + m_length; }

	bool operator==(const StringRef &rhs) const {
		return m_length == rhs.m_length && compare(rhs) == 0;
	}
	bool operator<(const StringRef &rhs) const { return compare(rhs) < 0; }

	StringRef operator+(int offset) const {
		DASSERT(offset >= 0 && offset <= m_length);
		return StringRef(m_data + offset, m_length - offset);
	}

  private:
	const char *m_data;
	int m_length;
};

struct Tokenizer {
	explicit Tokenizer(const char *str, char delim = ' ') : m_str(str), m_delim(delim) {}

	StringRef next();
	bool finished() const { return *m_str == 0; }

  private:
	const char *m_str;
	char m_delim;
};

inline bool caseEqual(const StringRef a, const StringRef b) {
	return a.size() == b.size() && a.caseCompare(b) == 0;
}
inline bool caseNEqual(const StringRef a, const StringRef b) { return !caseEqual(a, b); }
inline bool caseLess(const StringRef a, const StringRef b) { return a.caseCompare(b) < 0; }

// TODO: StringRef for string32 ?
Maybe<string32> toUTF32(StringRef);
Maybe<string> toUTF8(const string32 &);

// Returns size of buffer big enough for conversion
// 0 may be returned is string is invalid
int utf8Length(const string32 &);
int utf32Length(const string &);

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

struct ListNode {
	ListNode() : next(-1), prev(-1) {}
	bool empty() const { return next == -1 && prev == -1; }

	int next, prev;
};

struct List {
	List() : head(-1), tail(-1) {}
	bool empty() const { return head == -1; }

	int head, tail;
};

// TODO: add functions to remove head / tail

template <class Object, ListNode Object::*member, class Container>
void listInsert(Container &container, List &list, int idx) NOINLINE;

template <class Object, ListNode Object::*member, class Container>
void listRemove(Container &container, List &list, int idx) NOINLINE;

// Assumes that node is disconnected
template <class Object, ListNode Object::*member, class Container>
void listInsert(Container &container, List &list, int idx) {
	ListNode &node = container[idx].*member;
	DASSERT(node.empty());

	node.next = list.head;
	if(list.head == -1)
		list.tail = idx;
	else
		(container[list.head].*member).prev = idx;
	list.head = idx;
}

// Assumes that node is on this list
template <class Object, ListNode Object::*member, class Container>
void listRemove(Container &container, List &list, int idx) {
	ListNode &node = container[idx].*member;
	int prev = node.prev, next = node.next;

	if(prev == -1) {
		list.head = next;
	} else {
		(container[node.prev].*member).next = next;
		node.prev = -1;
	}

	if(next == -1) {
		list.tail = prev;
	} else {
		(container[next].*member).prev = prev;
		node.next = -1;
	}
}

string toLower(const string &str);
pair<string, bool> execCommand(const string &cmd);

class FilePath;

class XMLNode;
class XMLDocument;

class TextFormatter;
class TextParser;
}

#endif
