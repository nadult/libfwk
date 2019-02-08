// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "fwk/meta.h"
#include "fwk/operators.h"

#define NOINLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline))

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

#define FWK_STRINGIZE(...) FWK_STRINGIZE_(__VA_ARGS__)
#define FWK_STRINGIZE_(...) #__VA_ARGS__

#define FWK_JOIN(a, b) FWK_JOIN_(a, b)
#define FWK_JOIN_(a, b) a##b

namespace fwk {

using std::array;
using std::move;
using std::pair;
using std::string;
using std::swap;
using string32 = std::u32string;
using std::shared_ptr;

using std::begin;
using std::end;
using std::make_pair;
using std::make_shared;

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
constexpr NoAssertsTag no_asserts;

struct NoInitTag {};
constexpr NoInitTag no_init;

struct InvalidTag {};
constexpr InvalidTag invalid;

struct SentinelTag {};
constexpr SentinelTag sentinel;

// Idea from: Range V3
struct PriorityTag0 {};
struct PriorityTag1 : PriorityTag0 {};
struct PriorityTag2 : PriorityTag1 {};
struct PriorityTag3 : PriorityTag2 {};
struct PriorityTag4 : PriorityTag3 {};
using PriorityTagMax = PriorityTag4;

template <class> class Expected;
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
[[noreturn]] void assertFailed(const char *file, int line, const char *str);
[[noreturn]] void checkFailed(const char *file, int line, const char *fmt, ...) ATTRIB_PRINTF(3, 4);
[[noreturn]] void checkFailed(const char *file, int line, Error);

void handleCtrlC(void (*handler)());
void handleSegFault();

pair<string, bool> execCommand(const string &cmd);
void logError(const string &error);

int threadId();
void sleep(double sec);
double getTime();

#define FWK_FATAL(...) fwk::fatalError(__FILE__, __LINE__, __VA_ARGS__)

#define ASSERT(expr) ((!!(expr) || (fwk::assertFailed(__FILE__, __LINE__, FWK_STRINGIZE(expr)), 0)))
#define ASSERT_FAILED(...) fwk::assertFailed(__FILE__, __LINE__, __VA_ARGS__)

// Use CHECKs for checking input; If rollback mode is on, it will cause rollback()
// Otherwise it works just like an assert
#define CHECK(expr)                                                                                \
	(!!(expr) || (fwk::checkFailed(__FILE__, __LINE__, "%s", FWK_STRINGIZE(expr)), 0))
#define CHECK_FAILED(...) fwk::checkFailed(__FILE__, __LINE__, __VA_ARGS__)

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

// TODO: use std::is_trivially_copyable instead?
#define SERIALIZE_AS_POD(type)                                                                     \
	namespace fwk {                                                                                \
		template <> struct SerializeAsPod<type> { static constexpr bool value = true; };           \
	}

class Backtrace;
class Stream;
class FileStream;
class FilePath;

template <class T> class immutable_ptr;
template <class T> class immutable_weak_ptr;

class BaseVector;
template <class T> class PodVector;
template <class T> class Vector;
template <class T> using vector = Vector<T>;
template <class T> constexpr int type_size<Vector<T>> = 16;

template <class T> struct SerializeAsPod;

template <typename... Types> class Variant;
template <class T> class Maybe;
template <class T> class MaybeRef;
template <class T> using MaybeCRef = MaybeRef<const T>;

class Str;
class ZStr;
class FilePath;

class CXmlNode;
class XmlNode;
class XmlDocument;

class TextFormatter;
class TextParser;

class InputEvent;
class InputState;

template <class T> class ResourceLoader;
template <class T, class Constructor = ResourceLoader<T>> class ResourceManager;

template <class Key, class Value> class HashMap;
template <class Key, class Value>
constexpr int type_size<HashMap<Key, Value>> = sizeof(void *) == 4 ? 28 : 32;

template <class Key> class HashSet;
template <class Key> constexpr int type_size<HashSet<Key>> = sizeof(void *) == 4 ? 28 : 32;

template <class T> class UniquePtr;

class Any;
class AnyRef;

template <> constexpr int type_size<AnyRef> = 16;
template <> constexpr int type_size<Any> = 16;

// Implemented in logger.[h|cpp]
void log(Str message, Str unique_key);
void log(Str message);
bool logKeyPresent(Str);
}
