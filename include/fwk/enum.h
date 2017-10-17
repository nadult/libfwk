// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include "fwk_maybe.h"
#include "fwk_range.h"

namespace fwk {

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

template <class T> struct EnumFlags;

template <class Enum, class T> class EnumMap;
}
