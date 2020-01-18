// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "fwk/meta/operator.h"

#define NOINLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline))

// Exception-related attributes (supported only when compiling on clang):
//
// EXCEPT means that function can raise exceptions by adding them to per-thread exception stack.
// Putting it on a class adds it to every member (member attribute has higher priority though).
//
// By default every function is NOEXCEPT; You can use NOEXCEPT if checker marks some function
// as EXCEPT even though it doesn't raise exceptions. When you add NOEXCEPT, you're responsible
// for making sure that when function returns, no exceptions are raised.
//
// INST_EXCEPT can be used on templated functions to inform that some of the instantiations may
// raise exceptions. It should be used sparingly (best example is Vector::emplace_back).
#ifdef __clang__
#define EXCEPT __attribute__((annotate("except")))
#define INST_EXCEPT __attribute__((annotate("inst_except")))
#define NOEXCEPT __attribute__((annotate("not_except")))
#else
#define EXCEPT
#define INST_EXCEPT
#define NOEXCEPT
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

#define FWK_ASSIGN_RECONSTRUCT(Class)                                                              \
	Class &Class::operator=(const Class &rhs) {                                                    \
		if(this != &rhs) {                                                                         \
			this->~Class();                                                                        \
			new(this) Class(rhs);                                                                  \
		}                                                                                          \
		return *this;                                                                              \
	}

#define FWK_MOVE_ASSIGN_RECONSTRUCT(Class)                                                         \
	Class &Class::operator=(Class &&rhs) {                                                         \
		if(this != &rhs) {                                                                         \
			this->~Class();                                                                        \
			new(this) Class(move(rhs));                                                            \
		}                                                                                          \
		return *this;                                                                              \
	}

#define FWK_STRINGIZE(...) FWK_STRINGIZE_(__VA_ARGS__)
#define FWK_STRINGIZE_(...) #__VA_ARGS__

#define FWK_JOIN(a, b) FWK_JOIN_(a, b)
#define FWK_JOIN_(a, b) a##b

namespace fwk {

using std::array;
using std::move;
using std::string;
using std::swap;
using string32 = std::u32string;
using std::shared_ptr;

using std::begin;
using std::end;
using std::make_shared;

template <class T1, class T2 = T1> using Pair = std::pair<T1, T2>;
using std::pair;

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

struct NoAssertsTag {};
struct NoInitTag {};
struct InvalidTag {};
struct SentinelTag {};

constexpr NoAssertsTag no_asserts;
constexpr NoInitTag no_init;
constexpr InvalidTag invalid;
constexpr SentinelTag sentinel;

// Idea from: Range V3
struct PriorityTag0 {};
struct PriorityTag1 : PriorityTag0 {};
struct PriorityTag2 : PriorityTag1 {};
struct PriorityTag3 : PriorityTag2 {};
struct PriorityTag4 : PriorityTag3 {};
using PriorityTagMax = PriorityTag4;

template <class> class Expected;
template <class T> using Ex = Expected<T>;

struct ErrorChunk;
struct Error;

// Use these to print values and type names in error messages
template <class... T> struct Undef;
template <auto... V> struct UndefVal;
template <class T> using UndefSize = UndefVal<sizeof(T)>;

template <class T, int size> constexpr int arraySize(T (&)[size]) { return size; }

template <class T, class T1, class T2>
constexpr bool inRange(const T &value, const T1 &begin, const T2 &end) {
	return value >= begin && value < end;
}

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

// TODO: move FWK_FATAL, check, etc to fwk_assert ?
// TODO: use Str ?
[[noreturn]] void fatalError(const char *file, int line, const char *fmt, ...) ATTRIB_PRINTF(3, 4);
[[noreturn]] void fatalError(const Error &);
[[noreturn]] void assertFailed(const char *file, int line, const char *str);
[[noreturn]] void failedNotInRange(int, int, int);

inline void checkInRange(int value, int begin, int end) {
	if(value < begin || value >= end)
		failedNotInRange(value, begin, end);
}

void handleCtrlC(void (*handler)());
void handleSegFault();

void logError(const string &error);

void sleep(double sec);
double getTime();

#define FWK_FATAL(...) fwk::fatalError(__FILE__, __LINE__, __VA_ARGS__)

#define ASSERT(expr)                                                                               \
	((__builtin_expect(!!(expr), true) ||                                                          \
	  (fwk::assertFailed(__FILE__, __LINE__, FWK_STRINGIZE(expr)), 0)))

#ifdef NDEBUG
#define DASSERT(expr) ((void)0)
#define IF_DEBUG(...) ((void)0)
#else
#define DASSERT(expr) ASSERT(expr)
#define IF_DEBUG(...) __VA_ARGS__
#endif

#if defined(FWK_PARANOID)
#define PASSERT(expr) ASSERT(expr)
#define IF_PARANOID(...) __VA_ARGS__
#else
#define PASSERT(expr) ((void)0)
#define IF_PARANOID(...) ((void)0)
#endif

class Backtrace;
class FileStream;
class MemoryStream;
class FilePath;

class BaseVector;
template <class T> class PodVector;
template <class T> class Vector;
template <class T> class SparseVector;
template <class T, int> class SmallVector;
template <class T, int> class StaticVector;
template <class T> using vector = Vector<T>;

// Different name to inform that memory may come from a pool
template <class T> using PoolVector = Vector<T>;

template <class T> struct SerializeAsPod;

template <typename... Types> class Variant;
template <class T> class Maybe;
template <class T> class MaybeRef;
template <class T> using MaybeCRef = MaybeRef<const T>;

class Str;
class ZStr;

class CXmlNode;
class XmlNode;
class XmlDocument;

class TextFormatter;
class TextParser;

class InputEvent;
class InputState;

template <class Key, class Value, class Policy = None> class HashMap;
template <class Key> class HashSet;
template <class T> class Dynamic;

class Any;
class AnyConfig;

template <class T> inline constexpr int type_size<SparseVector<T>> = sizeof(void *) == 8 ? 48 : 40;
template <class Key, class Value, class Policy>
inline constexpr int type_size<HashMap<Key, Value, Policy>> = sizeof(void *) == 4 ? 32 : 40;
template <class Key> inline constexpr int type_size<HashSet<Key>> = sizeof(void *) == 4 ? 28 : 32;
template <> inline constexpr int type_size<Any> = sizeof(void *) * 2;

template <class T, int min_size = 0> class Span;
template <class T> class SparseSpan;

// This kind of data can be safely serialized to/from binary format, byte by byte
template <class T>
inline constexpr bool is_flat_data = std::is_arithmetic<T>::value || std::is_enum<T>::value;
template <class T, int N> inline constexpr bool is_flat_data<array<T, N>> = is_flat_data<T>;

template <class T> auto &asPod(const T &value) { return *(const array<char, sizeof(T)> *)(&value); }
template <class T> auto &asPod(T &value) { return *(array<char, sizeof(T)> *)(&value); }

// Implemented in logger.[h|cpp]
void log(Str message, Str unique_key);
void log(Str message);
bool logKeyPresent(Str);

enum class Platform { linux, mingw, html };
#if defined(FWK_PLATFORM_LINUX)
inline constexpr auto platform = Platform::linux;
#elif defined(FWK_PLATFORM_MINGW)
inline constexpr auto platform = Platform::mingw;
#else
inline constexpr auto platform = Platform::html;
#endif
}
