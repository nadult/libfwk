// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_BASE_H
#define FWK_BASE_H

#include <algorithm>
#include <array>
#include <atomic>
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
	immutable_ptr(const T &rhs) : m_ptr(make_shared<const T>(rhs)) { incCounter(); }
	immutable_ptr(T &&rhs) : m_ptr(make_shared<const T>(move(rhs))) { incCounter(); }

	immutable_ptr() = default;
	immutable_ptr(const immutable_ptr &) = default;
	immutable_ptr(immutable_ptr &&) = default;
	immutable_ptr &operator=(const immutable_ptr &) = default;
	immutable_ptr &operator=(immutable_ptr &&) = default;

	const T &operator*() const { return m_ptr.operator*(); }
	const T *operator->() const { return m_ptr.operator->(); }
	const T *get() const { return m_ptr.get(); }

	// It will make a copy if the pointer is not unique
	T *mutate() {
		if(!m_ptr.unique()) {
			m_ptr = make_shared<const T>(*m_ptr.get());
			// TODO: make a copy if counter is too big (and reset the counter)
			incCounter();
		}
		return const_cast<T *>(m_ptr.get());
	}

	explicit operator bool() const { return !!m_ptr.get(); }
	bool operator==(const T *rhs) const { return m_ptr == rhs; }
	bool operator==(const immutable_ptr &rhs) const { return m_ptr == rhs.m_ptr; }
	bool operator<(const immutable_ptr &rhs) const { return m_ptr < rhs.m_ptr; }

	auto getKey() const { return (long long)*m_ptr.get(); }
	auto getWeakPtr() const { return std::weak_ptr<const T>(m_ptr); }

	operator shared_ptr<const T>() const { return m_ptr; }

  private:
	immutable_ptr(shared_ptr<const T> ptr) : m_ptr(move(ptr)) {}
	template <class T1, class... Args> friend immutable_ptr<T1> make_immutable(Args &&...);
	template <class T1> friend immutable_ptr<T1> make_immutable(T1 &&);
	template <class T1, class U>
	friend immutable_ptr<T1> static_pointer_cast(const immutable_ptr<U> &);
	friend class immutable_base<T>;
	friend class immutable_weak_ptr<T>;

	void incCounter() {
		static_assert(std::is_base_of<immutable_base<T>, T>::value, "");
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

template <class T, class U> immutable_ptr<T> static_pointer_cast(const immutable_ptr<U> &cp) {
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

	bool operator==(const immutable_weak_ptr &rhs) const {
		return !m_ptr.owner_before(rhs.m_ptr) && !rhs.m_ptr.owner_before(m_ptr) &&
			   m_mutation_counter == rhs.m_mutation_counter;
	}
	bool operator<(const immutable_weak_ptr &rhs) const {
		if(m_mutation_counter == rhs.m_mutation_counter)
			return m_ptr.owner_before(rhs.m_ptr);
		return m_mutation_counter < rhs.m_mutation_counter;
	}
	bool expired() const { return m_ptr.expired(); }

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

class Stream;
template <class T> struct SerializeAsPod;

// TODO: use std::is_trivially_copyable instead?
#define SERIALIZE_AS_POD(type)                                                                     \
	namespace fwk {                                                                                \
		template <> struct SerializeAsPod<type> {                                                  \
			enum { value = 1 };                                                                    \
		};                                                                                         \
	}

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
