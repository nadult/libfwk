// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_BASE_H
#define FWK_BASE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#if _MSC_VER
#define NORETURN
#define NOINLINE
#define __restrict__ __restrict
#define ALWAYS_INLINE
#else
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline))
#endif

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

template <class... T> struct Undefined;
template <long long... V> struct UndefinedVal;

template <class T, int size> constexpr int arraySize(T (&)[size]) noexcept { return size; }

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

class SimpleAllocatorBase {
  public:
	void *allocateBytes(size_t count) noexcept;
	void deallocateBytes(void *ptr) noexcept { free(ptr); }
};

// TODO: in multithreaded env consider: tbb::scalable_allocator
template <class T> class SimpleAllocator : public SimpleAllocatorBase {
  public:
	using value_type = T;

	SimpleAllocator() = default;
	template <typename U> SimpleAllocator(const SimpleAllocator<U> &other) {}

	T *allocate(size_t count) noexcept {
		return static_cast<T *>(allocateBytes(count * sizeof(T)));
	}

	size_t max_size() const noexcept { return std::numeric_limits<size_t>::max() / sizeof(T); }

	template <class Other> struct rebind { using other = SimpleAllocator<Other>; };

	void deallocate(T *ptr, size_t) noexcept { deallocateBytes(ptr); }
	template <class U> bool operator==(const SimpleAllocator<U> &rhs) const { return true; }
	template <class Other> bool operator==(const Other &rhs) const { return false; }
};

#ifdef FWK_STD_VECTOR
template <class T> using vector = std::vector<T, SimpleAllocator<T>>;
#endif

template <class T> class immutable_ptr;
template <class T> class immutable_weak_ptr;

// TODO: enable_shared_from_this should be hidden
template <class T> class immutable_base : public std::enable_shared_from_this<const T> {
  public:
	immutable_base() : m_mutation_counter(-1) {}
	immutable_base(const immutable_base &) : immutable_base() {}
	immutable_base(immutable_base &&) : immutable_base() {}
	void operator=(const immutable_base &) {}
	void operator=(immutable_base &&) {}

	immutable_ptr<T> get_immutable_ptr() const;

  private:
	std::atomic<int> m_mutation_counter; // TODO: this should be atomic?
	friend class immutable_ptr<T>;
};

// Pointer to immutable object
// TODO: verify that it is thread-safe
// It can be mutated with mutate method, but if the pointer is
// not unique then a copy will be made.
// TODO: make it work without immutable_base
template <class T> class immutable_ptr {
  public:
	static_assert(std::is_base_of<immutable_base<T>, T>::value, "");

	immutable_ptr(const T &rhs) : m_ptr(make_shared<const T>(rhs)) { incCounter(); }
	immutable_ptr(T &&rhs) : m_ptr(make_shared<const T>(move(rhs))) { incCounter(); }

	immutable_ptr() = default;
	immutable_ptr(const immutable_ptr &) = default;
	immutable_ptr(immutable_ptr &&) = default;
	immutable_ptr &operator=(const immutable_ptr &) = default;
	immutable_ptr &operator=(immutable_ptr &&) = default;

	const T &operator*() const noexcept { return m_ptr.operator*(); }
	const T *operator->() const noexcept { return m_ptr.operator->(); }
	const T *get() const noexcept { return m_ptr.get(); }

	// It will make a copy if the pointer is not unique
	T *mutate() {
		if(!m_ptr.unique()) {
			m_ptr = make_shared<const T>(*m_ptr.get());
			// TODO: make a copy if counter is too big (and reset the counter)
			incCounter();
		}
		return const_cast<T *>(m_ptr.get());
	}

	explicit operator bool() const noexcept { return !!m_ptr.get(); }
	bool operator==(const T *rhs) const noexcept { return m_ptr == rhs; }
	bool operator==(const immutable_ptr &rhs) const noexcept { return m_ptr == rhs.m_ptr; }
	bool operator<(const immutable_ptr &rhs) const noexcept { return m_ptr < rhs.m_ptr; }

	auto getKey() const { return (long long)*m_ptr.get(); }
	auto getWeakPtr() const { return std::weak_ptr<const T>(m_ptr); }

	operator shared_ptr<const T>() const { return m_ptr; }

  private:
	immutable_ptr(shared_ptr<const T> ptr) : m_ptr(move(ptr)) {}
	template <class T1, class... Args> friend immutable_ptr<T1> make_immutable(Args &&...);
	template <class T1> friend immutable_ptr<T1> make_immutable(T1 &&);
	template <class T1, class U>
	friend immutable_ptr<T1> static_pointer_cast(const immutable_ptr<U> &) noexcept;
	friend class immutable_base<T>;
	friend class immutable_weak_ptr<T>;

	void incCounter() {
		const immutable_base<T> *base = m_ptr.get();
		const_cast<immutable_base<T> *>(base)->m_mutation_counter++;
	}
	int numMutations() const {
		const immutable_base<T> *base = m_ptr.get();
		return base->m_mutation_counter;
	}

	shared_ptr<const T> m_ptr;
};

template <class T> immutable_ptr<T> immutable_base<T>::get_immutable_ptr() const {
	if(m_mutation_counter >= 0)
		return immutable_ptr<T>(std::enable_shared_from_this<const T>::shared_from_this());
	return immutable_ptr<T>();
}

template <class T> inline T *mutate(immutable_ptr<T> &ptr) { return ptr.mutate(); }

template <class T> immutable_ptr<T> make_immutable(T &&object) {
	auto ret = immutable_ptr<T>(make_shared<const T>(move(object)));
	ret.incCounter();
	return ret;
}

template <class T, class... Args> immutable_ptr<T> make_immutable(Args &&... args) {
	auto ret = immutable_ptr<T>(make_shared<const T>(std::forward<Args>(args)...));
	ret.incCounter();
	return ret;
}

template <class T, class U>
immutable_ptr<T> static_pointer_cast(const immutable_ptr<U> &cp) noexcept {
	return immutable_ptr<T>(static_pointer_cast<const T>(cp.m_ptr));
}

template <class T> class immutable_weak_ptr {
  public:
	immutable_weak_ptr() = default;
	immutable_weak_ptr(immutable_ptr<T> ptr)
		: m_ptr(ptr.m_ptr), m_mutation_counter(ptr ? ptr.numMutations() : -1) {}

	immutable_ptr<T> lock() const {
		immutable_ptr<T> out(m_ptr.lock());
		if(out && out.numMutations() == m_mutation_counter)
			return out;
		return immutable_ptr<T>();
	}

	bool operator==(const immutable_weak_ptr &rhs) const noexcept {
		return !m_ptr.owner_before(rhs.m_ptr) && !rhs.m_ptr.owner_before(m_ptr) &&
			   m_mutation_counter == rhs.m_mutation_counter;
	}
	bool operator<(const immutable_weak_ptr &rhs) const noexcept {
		if(m_mutation_counter == rhs.m_mutation_counter)
			return m_ptr.owner_before(rhs.m_ptr);
		return m_mutation_counter < rhs.m_mutation_counter;
	}
	bool expired() const noexcept { return m_ptr.expired(); }

  private:
	std::weak_ptr<const T> m_ptr;
	int m_mutation_counter;
};

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

// TODO: use lib-lldb
class Backtrace {
  public:
	Backtrace(std::vector<void *> addresses, std::vector<string> symbols, pair<string, bool>);
	Backtrace(std::vector<void *> addresses, std::vector<string> symbols);
	Backtrace() = default;

	// If available, gdb backtraces will be used (which are more accurate)
	static Backtrace get(size_t skip = 0, void *context = nullptr, bool use_gdb = true);

	static pair<string, bool> gdbBacktrace(int skip_frames = 0) NOINLINE;

	// When filter is true, analyzer uses c++filt program to demangle C++
	// names; it also shortens some of the common long class names, like
	// std::basic_string<...> to fwk::string
	string analyze(bool filter) const;
	auto size() const { return m_addresses.size(); }

  private:
	static string filter(const string &);

	std::vector<void *> m_addresses;
	std::vector<string> m_symbols;
	pair<string, bool> m_gdb_result;
	bool m_use_gdb = false;
};

class Exception : public std::exception {
  public:
	explicit Exception(string text);
	explicit Exception(string text, Backtrace);
	~Exception() noexcept = default;

	const char *what() const noexcept override;
	const char *text() const noexcept { return m_text.c_str(); }
	string backtrace(bool filter = true) const { return m_backtrace.analyze(filter); }
	const Backtrace &backtraceData() const { return m_backtrace; }

  protected:
	string m_text;
	Backtrace m_backtrace;
};

#define FWK_STRINGIZE(...) FWK_STRINGIZE_(__VA_ARGS__)
#define FWK_STRINGIZE_(...) #__VA_ARGS__

#ifdef __clang__
__attribute__((__format__(__printf__, 3, 4)))
#endif
void throwException(const char *file, int line, const char *fmt, ...);
#ifdef __clang__
__attribute__((__format__(__printf__, 3, 4)))
#endif
void fatalError(const char *file, int line, const char *fmt, ...) NORETURN;
void assertFailed(const char *file, int line, const char *str) NORETURN;
void checkFailed(const char *file, int line, const char *str);
void handleCtrlC(void (*handler)());
void handleSegFault();
void sleep(double sec);
double getTime();

// TODO: maybe FILE / LINE is not required if we have backtraces?

#define FATAL(...) fwk::fatalError(__FILE__, __LINE__, __VA_ARGS__)
#define THROW(...) fwk::throwException(__FILE__, __LINE__, __VA_ARGS__)

// TODO: add fatal exception and use it for asserts
// TODO: add special assert for verifying input files, which throws normal exception

#define ASSERT(expr) ((!!(expr) || (fwk::assertFailed(__FILE__, __LINE__, FWK_STRINGIZE(expr)), 0)))

// Use this for checking input; It will throw on error, so that recovery is possible
#define CHECK(expr) (!!(expr) || (fwk::checkFailed(__FILE__, __LINE__, FWK_STRINGIZE(expr)), 0))

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

#ifndef FWK_STD_VECTOR
#include "fwk_vector.h"
#endif

#include "fwk_maybe.h"
#include "fwk_range.h"

#include "fwk_index_range.h"

namespace fwk {

void logError(const string &error);

// TODO: change name to borrowedstring ? (like in Rust)
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

int enumFromString(const char *str, CSpan<const char *> enum_strings, bool throw_on_invalid);

template <class Type> class EnumRange {
  public:
	class Iter {
	  public:
		Iter(int pos) : pos(pos) {}

		auto operator*() const { return Type(pos); }
		const Iter &operator++() {
			pos++;
			return *this;
		}

		bool operator<(const Iter &rhs) const { return pos < rhs.pos; }
		bool operator==(const Iter &rhs) const { return pos == rhs.pos; }

	  private:
		int pos;
	};

	auto begin() const { return Iter(m_min); }
	auto end() const { return Iter(m_max); }
	int size() const { return m_max - m_min; }

	EnumRange(int min, int max) : m_min(min), m_max(max) { DASSERT(min >= 0 && max >= min); }

  protected:
	int m_min, m_max;
};

namespace detail {

	template <unsigned...> struct Seq { using type = Seq; };
	template <unsigned N, unsigned... Is> struct GenSeqX : GenSeqX<N - 1, N - 1, Is...> {};
	template <unsigned... Is> struct GenSeqX<0, Is...> : Seq<Is...> {};
	template <unsigned N> using GenSeq = typename GenSeqX<N>::type;

	template <int N> struct Buffer { char data[N + 1]; };
	template <int N> struct Offsets { const char *data[N]; };

	constexpr bool isWhiteSpace(char c) {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
	}

	constexpr const char *skipZeros(const char *t) {
		while(!*t)
			t++;
		return t;
	}
	constexpr const char *skipToken(const char *t) {
		while(*t)
			t++;
		return t;
	}

	constexpr int countTokens(const char *t) {
		while(isWhiteSpace(*t))
			t++;
		if(!*t)
			return 0;

		int out = 1;
		while(*t) {
			if(*t == ',')
				out++;
			t++;
		}
		return out;
	}

	template <unsigned N> struct OffsetsGen : public OffsetsGen<N - 1> {
		constexpr OffsetsGen(const char *current_ptr)
			: OffsetsGen<N - 1>(skipZeros(skipToken(current_ptr))), ptr(current_ptr) {}
		const char *ptr;
	};

	template <> struct OffsetsGen<1> {
		constexpr OffsetsGen(const char *ptr) : ptr(ptr) {}
		const char *ptr;
	};

	template <int N, unsigned... IS>
	constexpr Buffer<N> zeroWhiteSpace(const char (&t)[N], Seq<IS...>) {
		return {{(isWhiteSpace(t[IS]) ? '\0' : t[IS])...}};
	}

	template <int N> constexpr Buffer<N> zeroWhiteSpace(const char (&t)[N]) {
		return zeroWhiteSpace(t, GenSeq<N>());
	}

	template <class T, unsigned... IS> constexpr auto makeOffsets(T t, Seq<IS...>) {
		constexpr const char *start = skipZeros(t());
		constexpr unsigned num_tokens = sizeof...(IS);
		constexpr const auto oseq = OffsetsGen<num_tokens>(start);
		return Offsets<sizeof...(IS)>{{((const OffsetsGen<num_tokens - IS> &)oseq).ptr...}};
	}
}

// Safe enum class
// Initially initializes to 0 (first element). Converts to int, can be easily used as
// an index into some array. Can be converted to/from strings, which are automatically generated
// from enum names. fwk enum cannot be defined in class or function scope. Some examples are
// available in src/test/enums.cpp.
#define DEFINE_ENUM(id, ...)                                                                       \
	enum class id : unsigned char { __VA_ARGS__ };                                                 \
	inline auto enumStrings(id) {                                                                  \
		using namespace fwk::detail;                                                               \
		static constexpr const auto s_buffer = zeroWhiteSpace(#__VA_ARGS__);                       \
		static constexpr const auto s_offsets =                                                    \
			makeOffsets([] { return s_buffer.data; }, GenSeq<countTokens(#__VA_ARGS__)>());        \
		constexpr int size = fwk::arraySize(s_offsets.data);                                       \
		static_assert(size <= 64, "Maximum number of enum elements is 64");                        \
		return fwk::CSpan<const char *, size>(s_offsets.data, size);                               \
	}

struct NotAnEnum;

namespace detail {
	template <class T> struct IsEnum {
		template <class C> static auto test(int) -> decltype(enumStrings(C()));
		template <class C> static auto test(...) -> void;
		enum { value = !std::is_same<void, decltype(test<T>(0))>::value };
	};
}

template <class T> constexpr bool isEnum() { return detail::IsEnum<T>::value; }

template <class T> using EnableIfEnum = EnableIf<isEnum<T>(), NotAnEnum>;

template <class T, EnableIfEnum<T>...> static T fromString(const char *str) {
	return T(fwk::enumFromString(str, enumStrings(T()), true));
}

template <class T, EnableIfEnum<T>...> static T fromString(const string &str) {
	return fromString<T>(str.c_str());
}

template <class T, EnableIfEnum<T>...> static Maybe<T> tryFromString(const char *str) {
	int ret = fwk::enumFromString(str, enumStrings(T()), false);
	if(ret == -1)
		return none;
	return T(ret);
}

template <class T, EnableIfEnum<T>...> static Maybe<T> tryFromString(const string &str) {
	return tryFromString<T>(str.c_str());
}
template <class T, EnableIfEnum<T>...> static const char *toString(T value) {
	return enumStrings(T())[(int)value];
}

template <class T, EnableIfEnum<T>...> constexpr int count() {
	return decltype(enumStrings(T()))::minimum_size;
}

template <class T, EnableIfEnum<T>...> auto all() { return EnumRange<T>(0, count<T>()); }

template <class T, EnableIfEnum<T>...> T next(T value) { return T((int(value) + 1) % count<T>()); }

template <class T, EnableIfEnum<T>...> T prev(T value) {
	return T((int(value) + (count<T> - 1)) % count<T>());
}

template <class T, EnableIfEnum<T>...>
using BestFlagsBase =
	Conditional<count<T>() <= 8, u8,
				Conditional<count<T>() <= 16, u16, Conditional<count<T>() <= 32, u32, u64>>>;

template <class T, class Base_ = BestFlagsBase<T>> struct EnumFlags {
	using Base = Base_;
	enum { max_flags = fwk::count<T>() };
	static constexpr const Base mask =
		(Base(1) << (max_flags - 1)) - Base(1) + (Base(1) << (max_flags - 1));

	static_assert(isEnum<T>(), "EnumFlags<> should be based on fwk-enum");
	static_assert(std::is_unsigned<Base>::value, "");
	static_assert(fwk::count<T>() <= sizeof(Base) * 8, "");

	constexpr EnumFlags() : bits(0) {}
	constexpr EnumFlags(None) : bits(0) {}
	constexpr EnumFlags(T value) : bits(Base(1) << uint(value)) {}
	constexpr explicit EnumFlags(Base bits) : bits(bits) {}
	template <class TBase> constexpr EnumFlags(EnumFlags<T, TBase> rhs) : bits(rhs.bits) {}

	constexpr EnumFlags operator|(EnumFlags rhs) const { return EnumFlags(bits | rhs.bits); }
	constexpr EnumFlags operator&(EnumFlags rhs) const { return EnumFlags(bits & rhs.bits); }
	constexpr EnumFlags operator^(EnumFlags rhs) const { return EnumFlags(bits ^ rhs.bits); }
	constexpr EnumFlags operator~() const { return EnumFlags((~bits) & (mask)); }

	constexpr void operator|=(EnumFlags rhs) { bits |= rhs.bits; }
	constexpr void operator&=(EnumFlags rhs) { bits &= rhs.bits; }
	constexpr void operator^=(EnumFlags rhs) { bits ^= rhs.bits; }

	constexpr bool operator==(T rhs) const { return bits == (Base(1) << int(rhs)); }
	constexpr bool operator==(EnumFlags rhs) const { return bits == rhs.bits; }
	constexpr bool operator<(EnumFlags rhs) const { return bits < rhs.bits; }

	constexpr explicit operator bool() const { return bits != 0; }

	static constexpr EnumFlags all() { return EnumFlags(mask); }

	Base bits;
};

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> flag(T val) {
	return EnumFlags<T>(val);
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator|(T lhs, T rhs) {
	return EnumFlags<T>(lhs) | rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator&(T lhs, T rhs) {
	return EnumFlags<T>(lhs) & rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator^(T lhs, T rhs) {
	return EnumFlags<T>(lhs) ^ rhs;
}

template <class T, EnableIfEnum<T>...> constexpr EnumFlags<T> operator~(T bit) {
	return ~EnumFlags<T>(bit);
}

namespace detail {
	template <class T> struct IsEnumFlags {
		enum { value = 0 };
	};
	template <class T, class Base> struct IsEnumFlags<EnumFlags<T, Base>> {
		enum { value = 1 };
	};
}

template <class T> constexpr bool isEnumFlags() { return detail::IsEnumFlags<T>::value; }

template <class T> using EnableIfEnumFlags = EnableIf<detail::IsEnumFlags<T>::value>;

template <class TFlags, EnableIfEnumFlags<TFlags>...> constexpr int countBits(const TFlags &flags) {
	int out = 0;
	for(int i = 0; i < TFlags::max_flags; i++)
		out += flags.bits & (typename TFlags::Base(1) << i) ? 1 : 0;
	return out;
}

template <class Enum, class T> class EnumMap {
  public:
	static_assert(isEnum<Enum>(),
				  "EnumMap<> can only be used for enums specified with DEFINE_ENUM");

	EnumMap(CSpan<pair<Enum, T>> pairs) {
#ifndef NDEBUG
		bool enum_used[count<Enum>()] = {
			false,
		};
		int enum_count = 0;
#endif
		for(auto &pair : pairs) {
			int index = (int)pair.first;
			m_data[index] = pair.second;
#ifndef NDEBUG
			DASSERT(!enum_used[index] && "Enum entry missing");
			enum_used[index] = true;
			enum_count++;
#endif
		}
		DASSERT(enum_count == count<Enum>() && "Invalid number of pairs specified");
	}
	EnumMap(CSpan<pair<Enum, T>> pairs, T default_value) {
		m_data.fill(default_value);
		for(auto &pair : pairs)
			m_data[(int)pair.first] = pair.second;
	}
	EnumMap(CSpan<T> values) {
		DASSERT(values.size() == (int)m_data.size() && "Invalid number of values specified");
		std::copy(values.begin(), values.end(), m_data.begin());
	}
	explicit EnumMap(const T &default_value = T()) { m_data.fill(default_value); }
	EnumMap(std::initializer_list<pair<Enum, T>> list) : EnumMap(CSpan<pair<Enum, T>>(list)) {}
	EnumMap(std::initializer_list<pair<Enum, T>> list, T default_value)
		: EnumMap(CSpan<pair<Enum, T>>(list), default_value) {}
	EnumMap(std::initializer_list<T> values) : EnumMap(CSpan<T>(values)) {}

	const T &operator[](Enum index) const { return m_data[(int)index]; }
	T &operator[](Enum index) { return m_data[(int)index]; }

	T *begin() { return m_data.data(); }
	T *end() { return m_data.data() + size(); }
	const T *begin() const { return m_data.data(); }
	const T *end() const { return m_data.data() + size(); }
	bool operator==(const EnumMap &rhs) const { return std::equal(begin(), end(), rhs.begin()); }
	bool operator<(const EnumMap &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	constexpr bool empty() const { return size() == 0; }
	constexpr int size() const { return count<Enum>(); }
	void fill(const T &value) {
		for(int n = 0; n < size(); n++)
			m_data[n] = value;
	}

  private:
	array<T, count<Enum>()> m_data;
};

#define SAFE_ARRAY(declaration, size, ...)                                                         \
	declaration[] = {__VA_ARGS__};                                                                 \
	static_assert(COUNT_ARGUMENTS(__VA_ARGS__) == size, "Invalid number of elements in an array");

template <class T> struct SerializeAsPod;

class Stream {
  public:
	Stream(bool is_loading);
	virtual ~Stream() noexcept {}

	virtual const char *name() const noexcept { return ""; }

	long long size() const noexcept { return m_size; }
	long long pos() const noexcept { return m_pos; }

	void saveData(const void *ptr, int bytes);
	void loadData(void *ptr, int bytes);
	void seek(long long pos);

	int loadString(char *buffer, int size);
	void saveString(const char *, int size = 0);

	bool isLoading() const noexcept { return m_is_loading; }
	bool isSaving() const noexcept { return !m_is_loading; }
	bool allOk() const noexcept { return !m_exception_thrown; }

	//! Serializes 32-bit signature; While saving, it simply writes it to stream,
	//! while loading it checks if loaded signature is equal to sig, if not
	//! it throws an exception
	void signature(u32 sig);

	// Max length: 32
	void signature(const char *sig, int length);

	template <class... Args> void unpack(Args &... args) {
		char buffer[SumSize<Args...>::value];
		loadData(buffer, sizeof(buffer));
		CopyFrom<Args...>::copy(buffer, args...);
	}

	template <class... Args> void pack(const Args &... args) {
		char buffer[SumSize<Args...>::value];
		CopyTo<Args...>::copy(buffer, args...);
		saveData(buffer, sizeof(buffer));
	}

	template <class T> Stream &operator<<(const T &obj) {
		saveToStream(obj, *this);
		return *this;
	}

	template <class T> Stream &operator>>(T &obj) {
		loadFromStream(obj, *this);
		return *this;
	}

	void handleException(const Exception &) NOINLINE;

  private:
	template <class... Args> struct SumSize {
		enum { value = 0 };
	};
	template <class T, class... Args> struct SumSize<T, Args...> {
		enum { value = sizeof(T) + SumSize<Args...>::value };
	};
	template <class T> struct SumSize<T> {
		enum { value = sizeof(T) };
	};

	template <class... Args> struct CopyTo {};
	template <class T1> struct CopyTo<T1> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, const T1 &src) { *(T1 *)ptr = src; }
	};
	template <class T1, class... Args> struct CopyTo<T1, Args...> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, const T1 &src, const Args &... args) {
			*(T1 *)ptr = src;
			CopyTo<Args...>::copy((char *)ptr + sizeof(T1), args...);
		}
	};

	template <class... Args> struct CopyFrom {};
	template <class T1> struct CopyFrom<T1> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, T1 &dst) { dst = *(T1 *)ptr; }
	};
	template <class T1, class... Args> struct CopyFrom<T1, Args...> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, T1 &dst, Args &... args) {
			dst = *(T1 *)ptr;
			CopyFrom<Args...>::copy((char *)ptr + sizeof(T1), args...);
		}
	};

	template <class T, bool pod> struct Serialization {
		static void doLoad(T &obj, Stream &sr) {
			try {
				obj.load(sr);
			} catch(const Exception &ex) {
				sr.handleException(ex);
			}
		}
		static void doSave(const T &obj, Stream &sr) {
			try {
				obj.save(sr);
			} catch(const Exception &ex) {
				sr.handleException(ex);
			}
		}
	};

	template <class T> struct Serialization<T, true> {
		static void doLoad(T &obj, Stream &sr) { sr.loadData(&obj, sizeof(T)); }
		static void doSave(const T &obj, Stream &sr) { sr.saveData(&obj, sizeof(T)); }
	};

	template <class T, int tsize, bool pod> struct Serialization<T[tsize], pod> {
		static void doLoad(T (&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].load(sr);
		};
		static void doSave(const T (&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].save(sr);
		};
	};

	template <class T, int tsize> struct Serialization<T[tsize], true> {
		static void doLoad(T (&obj)[tsize], Stream &sr) { sr.loadData(&obj[0], sizeof(T) * tsize); }
		static void doSave(const T (&obj)[tsize], Stream &sr) {
			sr.saveData(&obj[0], sizeof(T) * tsize);
		}
	};

	template <class T> friend void loadFromStream(T &, Stream &);
	template <class T> friend void saveToStream(const T &, Stream &);

	friend class Loader;
	friend class Saver;

  protected:
	virtual void v_load(void *, int) { FATAL("v_load unimplemented"); }
	virtual void v_save(const void *, int) { FATAL("v_save unimplemented"); }
	virtual void v_seek(long long pos) { m_pos = pos; }

	long long m_size, m_pos;
	bool m_exception_thrown, m_is_loading;
};

// You can derive your classes from this
struct TraitSerializeAsPod {};

// You can specialize this for your classes
template <class T> struct SerializeAsPod {
	template <typename U, void (U::*)(Stream &)> struct SFINAE {};

	template <typename U> static char test(SFINAE<U, &U::load> *);
	template <typename U> static char test(SFINAE<U, &U::save> *);
	template <typename U> static long test(...);

	enum { has_serialize_method = sizeof(test<T>(0)) == 1 };
	enum {
		value = std::is_base_of<TraitSerializeAsPod, T>::value ||
				(std::is_pod<T>::value && !has_serialize_method)
	};
	enum { arraySize = 1 };
};

template <class T, int size> struct SerializeAsPod<T[size]> {
	enum { value = SerializeAsPod<T>::value };
	enum { arraySize = size };
};

// TODO: use std::is_trivially_copyable instead?
#define SERIALIZE_AS_POD(type)                                                                     \
	namespace fwk {                                                                                \
		template <> struct SerializeAsPod<type> {                                                  \
			enum { value = 1 };                                                                    \
		};                                                                                         \
	}

void loadFromStream(string &, Stream &);
void saveToStream(const string &, Stream &);

template <class T> void loadFromStream(vector<T> &v, Stream &sr) {
	u32 size;
	sr.loadData(&size, sizeof(size));
	try {
		v.resize(size);
	} catch(Exception &ex) {
		sr.handleException(ex);
	}

	if(SerializeAsPod<T>::value)
		sr.loadData(&v[0], sizeof(T) * size);
	else
		for(u32 n = 0; n < size; n++)
			loadFromStream(v[n], sr);
}

template <class T> void saveToStream(const vector<T> &v, Stream &sr) {
	u32 size;
	size = u32(v.size());
	if(size_t(size) != size_t(v.size()))
		sr.handleException(Exception("Vector size too big (> 2^32) for serializer to handle"));
	sr.saveData(&size, sizeof(size));

	if(SerializeAsPod<T>::value)
		sr.saveData(&v[0], sizeof(T) * size);
	else
		for(u32 n = 0; n < size; n++)
			saveToStream(v[n], sr);
}

template <class T> void loadFromStream(T &obj, Stream &sr) {
	Stream::template Serialization<T, SerializeAsPod<T>::value>::doLoad(obj, sr);
	static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
				  "Automatic serialization of pointers and references is not avaliable");
}

template <class T> void saveToStream(const T &obj, Stream &sr) {
	Stream::template Serialization<T, SerializeAsPod<T>::value>::doSave(obj, sr);
	static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
				  "Automatic serialization of pointers and references is not avaliable");
}

// Buffered, stdio based
class FileStream : public Stream {
  public:
	FileStream(const char *file_name, bool is_loading);
	FileStream(const string &file_name, bool is_loading)
		: FileStream(file_name.c_str(), is_loading) {}
	~FileStream() noexcept;

	FileStream(const FileStream &) = delete;
	void operator=(const FileStream &) = delete;

	const char *name() const noexcept override;

  protected:
	void v_load(void *, int) override;
	void v_save(const void *, int) override;
	void v_seek(long long) override;

	void *m_file;
	string m_name;
};

class Loader : public FileStream {
  public:
	Loader(const char *file_name) : FileStream(file_name, true) {}
	Loader(const string &file_name) : FileStream(file_name.c_str(), true) {}
};

class Saver : public FileStream {
  public:
	Saver(const char *file_name) : FileStream(file_name, false) {}
	Saver(const string &file_name) : FileStream(file_name.c_str(), false) {}
};

class MemoryLoader : public Stream {
  public:
	MemoryLoader(const char *data, long long size);
	MemoryLoader(vector<char> &data) : MemoryLoader(data.data(), data.size()) {}

  protected:
	void v_load(void *, int) override;

	const char *m_data;
};

class MemorySaver : public Stream {
  public:
	MemorySaver(char *data, long long size);
	MemorySaver(vector<char> &data) : MemorySaver(data.data(), data.size()) {}

  protected:
	void v_save(const void *, int) override;

	char *m_data;
};

template <class T> class ResourceLoader {
  public:
	ResourceLoader(const string &file_prefix, const string &file_suffix)
		: m_file_prefix(file_prefix), m_file_suffix(file_suffix) {}

	immutable_ptr<T> operator()(const string &name) const {
		Loader stream(fileName(name));
		return make_immutable<T>(name, stream);
	}

	string fileName(const string &name) const { return m_file_prefix + name + m_file_suffix; }
	const string &filePrefix() const { return m_file_prefix; }
	const string &fileSuffix() const { return m_file_suffix; }

  protected:
	string m_file_prefix, m_file_suffix;
};

template <class T, class Constructor = ResourceLoader<T>> class ResourceManager {
  public:
	using PResource = immutable_ptr<T>;

	template <class... ConstructorArgs>
	ResourceManager(ConstructorArgs &&... args)
		: m_constructor(std::forward<ConstructorArgs>(args)...) {}
	ResourceManager() {}
	~ResourceManager() {}

	const Constructor &constructor() const { return m_constructor; }

	PResource accessResource(const string &name) {
		auto it = m_dict.find(name);
		if(it == m_dict.end()) {
			PResource res = m_constructor(name);
			DASSERT(res);
			m_dict[name] = res;
			return res;
		}
		return it->second;
	}

	PResource findResource(const string &name) const {
		auto it = m_dict.find(name);
		if(it == m_dict.end())
			return PResource();
		return it->second;
	}

	PResource operator[](const string &name) { return accessResource(name); }

	const auto &dict() const { return m_dict; }

	PResource removeResource(const string &name) {
		auto iter = m_dict.find(name);
		if(iter != m_dict.end()) {
			m_dict.erase(iter);
			return iter.second;
		}
		return PResource();
	}

	void insertResource(const string &name, PResource res) { m_dict[name] = res; }

	void renameResource(const string &old_name, const string &new_name) {
		insertResource(new_name, move(removeResource(old_name)));
	}

	using Iterator = typename std::map<string, PResource>::const_iterator;

	auto begin() const { return std::begin(m_dict); }
	auto end() const { return std::end(m_dict); }

	void clear() { m_dict.clear(); }

  private:
	typename std::map<string, PResource> m_dict;
	Constructor m_constructor;
	string m_prefix, m_suffix;
};

template <class T> class ClonablePtr : public unique_ptr<T> {
  public:
	static_assert(std::is_same<decltype(&T::clone), T *(T::*)() const>::value, "");

	ClonablePtr(const ClonablePtr &rhs) : unique_ptr<T>(rhs ? rhs->clone() : nullptr) {}
	ClonablePtr(ClonablePtr &&rhs) : unique_ptr<T>(move(rhs)) {}
	ClonablePtr(T *ptr) : unique_ptr<T>(ptr) {}
	ClonablePtr() {}

	explicit operator bool() const { return unique_ptr<T>::operator bool(); }
	bool isValid() const { return unique_ptr<T>::operator bool(); }

	void operator=(ClonablePtr &&rhs) { unique_ptr<T>::operator=(move(rhs)); }
	void operator=(const ClonablePtr &rhs) {
		if(&rhs == this)
			return;
		T *clone = rhs ? rhs->clone() : nullptr;
		unique_ptr<T>::reset(clone);
	}
};

// Very simple and efficent vector for POD Types; Use with care:
// - user is responsible for initializing the data
// - when resizing, data is destroyed
// TODO: derive from PodArrayBase, resize, and serialize can be shared (modulo sizeof(T)
// multiplier)
template <class T> class PodArray {
  public:
	PodArray() : m_data(nullptr), m_size(0) {}
	explicit PodArray(int size) : m_data(nullptr), m_size(0) { resize(size); }
	PodArray(const PodArray &rhs) : m_size(rhs.m_size) {
		m_data = (T *)malloc(m_size * sizeof(T));
		memcpy(m_data, rhs.m_data, sizeof(T) * m_size);
	}
	PodArray(const T *data, int data_size) : m_size(data_size) {
		m_data = (T *)malloc(m_size * sizeof(T));
		memcpy(m_data, data, data_size * sizeof(T));
	}
	PodArray(PodArray &&rhs) : m_size(rhs.m_size), m_data(rhs.m_data) {
		rhs.m_data = nullptr;
		rhs.m_size = 0;
	}
	~PodArray() { resize(0); }

	void operator=(const PodArray &rhs) {
		if(&rhs == this)
			return;
		resize(rhs.m_size);
		memcpy(m_data, rhs.m_data, rhs.m_size);
	}
	void operator=(PodArray &&rhs) {
		if(&rhs == this)
			return;
		clear();
		m_data = rhs.m_data;
		m_size = rhs.m_size;
		rhs.m_data = nullptr;
		rhs.m_size = 0;
	}
	void load(Stream &sr) NOINLINE;
	void save(Stream &sr) const NOINLINE;
	void resize(int new_size) NOINLINE;

	void swap(PodArray &rhs) {
		fwk::swap(m_data, rhs.m_data);
		fwk::swap(m_size, rhs.m_size);
	}

#ifndef FWK_STD_VECTOR
	void swap(Vector<T> &rhs) { // TODO: some capacity may be wasted
		char *cur_data = reinterpret_cast<char *>(m_data);
		int cur_size = m_size;
		m_data = reinterpret_cast<T *>(rhs.m_base.data);
		m_size = rhs.m_base.size;
		rhs.m_base.data = cur_data;
		rhs.m_base.size = rhs.m_base.capacity = cur_size;
	}
#endif

	void clear() {
		m_size = 0;
		free(m_data);
		m_data = nullptr;
	}
	bool empty() const { return m_size == 0; }
	bool inRange(int index) const { return fwk::inRange(index, 0, m_size); }

	T *data() { return m_data; }
	const T *data() const { return m_data; }

	T *begin() { return m_data; }
	const T *begin() const { return m_data; }

	T *end() { return m_data + m_size; }
	const T *end() const { return m_data + m_size; }

	T &operator[](int idx) {
		PASSERT(inRange(idx));
		return m_data[idx];
	}
	const T &operator[](int idx) const {
		PASSERT(inRange(idx));
		return m_data[idx];
	}

	int size() const { return m_size; }
	int dataSize() const { return m_size * (int)sizeof(T); }

	operator Span<T>() { return {m_data, m_size}; }
	operator CSpan<T>() const { return {m_data, m_size}; }

  private:
	T *m_data;
	int m_size;
};

template <class T> void PodArray<T>::load(Stream &sr) {
	i32 size;
	sr >> size;
	ASSERT(size >= 0);

	resize(size);
	if(m_data)
		sr.loadData(m_data, sizeof(T) * m_size);
}

template <class T> void PodArray<T>::save(Stream &sr) const {
	sr << m_size;
	if(m_data)
		sr.saveData(m_data, sizeof(T) * m_size);
}

template <class T> void PodArray<T>::resize(int new_size) {
	DASSERT(new_size >= 0);
	if(m_size == new_size)
		return;

	clear();
	m_size = new_size;
	if(new_size)
		m_data = (T *)malloc(new_size * sizeof(T));
}

class BitVector {
  public:
	typedef u32 base_type;
	enum {
		base_shift = 5,
		base_size = 32,
	};

	struct Bit {
		Bit(base_type &base, int bit_index) : base(base), bit_index(bit_index) {}
		operator bool() const { return base & (base_type(1) << bit_index); }

		void operator=(bool value) {
			base = (base & ~(base_type(1) << bit_index)) | ((base_type)value << bit_index);
		}

	  protected:
		base_type &base;
		int bit_index;
	};

	explicit BitVector(int size = 0);
	void resize(int new_size, bool clear_value = false);

	int size() const { return m_size; }
	int baseSize() const { return m_data.size(); }

	void clear(bool value);

	const PodArray<base_type> &data() const { return m_data; }
	PodArray<base_type> &data() { return m_data; }

	bool operator[](int idx) const {
		PASSERT(inRange(idx, 0, m_size));
		return m_data[idx >> base_shift] & (1 << (idx & (base_size - 1)));
	}

	Bit operator[](int idx) {
		PASSERT(inRange(idx, 0, m_size));
		return Bit(m_data[idx >> base_shift], idx & (base_size - 1));
	}

	bool any(int base_idx) const { return m_data[base_idx] != base_type(0); }

	bool all(int base_idx) const { return m_data[base_idx] == ~base_type(0); }

  protected:
	PodArray<base_type> m_data;
	int m_size;
};

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

// TODO: verify that it works on windows when working on different drives
class FilePath {
  public:
	FilePath(const char *);
	FilePath(const string &);
	FilePath(FilePath &&);
	FilePath(const FilePath &);
	FilePath();

	void operator=(const FilePath &rhs) { m_path = rhs.m_path; }
	void operator=(FilePath &&rhs) { m_path = move(rhs.m_path); }

	bool isRoot() const;
	bool isAbsolute() const;
	bool empty() const { return m_path.empty(); }

	string fileName() const;
	string fileExtension() const;
	bool isDirectory() const;
	bool isRegularFile() const;

	FilePath relative(const FilePath &relative_to = current()) const;
	bool isRelative(const FilePath &ancestor) const;

	FilePath absolute() const;
	FilePath parent() const;

	FilePath operator/(const FilePath &other) const;
	FilePath &operator/=(const FilePath &other);

	static FilePath current();
	static void setCurrent(const FilePath &);

	operator const string &() const { return m_path; }
	const char *c_str() const { return m_path.c_str(); }
	int size() const { return (int)m_path.size(); }

	FWK_ORDER_BY(FilePath, m_path);

  private:
	struct Element {
		bool isDot() const;
		bool isDots() const;
		bool isRoot() const;
		bool operator==(const Element &rhs) const;

		const char *ptr;
		int size;
	};

	static Element extractRoot(const char *);
	static void divide(const char *, vector<Element> &);
	static void simplify(const vector<Element> &src, vector<Element> &dst);
	void construct(const vector<Element> &);

	string m_path; // its always non-empty
};

struct FileEntry {
	FilePath path;
	bool is_dir;

	bool operator<(const FileEntry &rhs) const {
		return is_dir == rhs.is_dir ? path < rhs.path : is_dir > rhs.is_dir;
	}
};

namespace FindFiles {
	enum Flags {
		regular_file = 1,
		directory = 2,

		recursive = 4,

		relative = 8, // all paths relative to given path
		absolute = 16, // all paths absolute
		include_parent = 32, // include '..'
	};
};

vector<string> findFiles(const string &prefix, const string &suffix);
vector<FileEntry> findFiles(const FilePath &path, int flags = FindFiles::regular_file);
bool removeSuffix(string &str, const string &suffix);
bool removePrefix(string &str, const string &prefix);
string toLower(const string &str);
void mkdirRecursive(const FilePath &path);
bool access(const FilePath &);
double lastModificationTime(const FilePath &);
FilePath executablePath();
pair<string, bool> execCommand(const string &cmd);

class XMLNode;
class XMLDocument;

class TextFormatter;
class TextParser;
}

#endif
