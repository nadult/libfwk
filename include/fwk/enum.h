// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/str.h"

namespace fwk {

namespace detail {

	int parseEnum(Str, const char *const *strings, int count, bool check_if_invalid) EXCEPT;
	int parseEnum(TextParser &, const char *const *strings, int count) EXCEPT;
	unsigned long long parseFlags(TextParser &, const char *const *strings, int count) EXCEPT;
	void formatFlags(unsigned long long, TextFormatter &, const char *const *strings, int count);

	static constexpr bool isWhiteSpace(char c) {
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
	}

	template <int N> struct EnumBuffer {
		constexpr EnumBuffer(const char *ptr) : data{} {
			for(int n = 0; n < N; n++)
				data[n] = isWhiteSpace(ptr[n]) ? 0 : ptr[n];
			data[N] = 0;
		}

		char data[N + 1];
	};

	template <int N, int K> struct EnumOffsets {
		constexpr EnumOffsets(const char *ptr) : data{} {
			bool prev_zero = true;
			int k = 0;
			for(int n = 0; n < N; n++) {
				if(ptr[n]) {
					if(prev_zero)
						data[k++] = ptr + n;
					prev_zero = false;
				} else {
					prev_zero = true;
				}
			}
		}

		const char *data[K];
	};

	template <const char *(*func)(), int TK> struct EnumStrings {
		static constexpr const char *init_str = func();

		static constexpr int len(const char *ptr) {
			int len = 0;
			while(ptr[len])
				len++;
			return len;
		}

		static constexpr int N = len(init_str);
		static constexpr int K = TK;

		static_assert(K <= 254, "Maximum number of enum elements is 254");

		static constexpr EnumBuffer<N> buffer = {init_str};
		static constexpr EnumOffsets<N, K> offsets = {buffer.data};
	};
}

template <class T> struct EnumFlags;
template <class Enum, class T> class EnumMap;

// Improved enum class: provides range access, counting, iteration. Can be converted to/from strings,
// which are automatically generated from enum names. fwk enum cannot be defined in function scope.
// Some examples are available in src/test/enums.cpp.
#define DEFINE_ENUM(id, ...)                                                                       \
	enum class id : unsigned char { __VA_ARGS__ };                                                 \
	inline auto enumStrings(id) {                                                                  \
		enum temp { __VA_ARGS__, _enum_count };                                                    \
		constexpr const char *(*func)() = [] { return #__VA_ARGS__; };                             \
		return fwk::detail::EnumStrings<func, _enum_count>();                                      \
	}

#define DEFINE_ENUM_MEMBER(id, ...)                                                                \
	enum class id : unsigned char { __VA_ARGS__ };                                                 \
	inline friend auto enumStrings(id) {                                                           \
		enum temp { __VA_ARGS__, _enum_count };                                                    \
		constexpr const char *(*func)() = [] { return #__VA_ARGS__; };                             \
		return fwk::detail::EnumStrings<func, _enum_count>();                                      \
	}

#define DECLARE_ENUM(id)                                                                           \
	enum class id : unsigned char;                                                                 \
	char _fwkDeclaredEnum(id);

struct NotAnEnum;

namespace detail {
	template <class T> struct IsEnum {
		FWK_SFINAE_TEST(value, T, enumStrings(DECLVAL(U)));
		FWK_SFINAE_TEST(decl_value, T, _fwkDeclaredEnum(DECLVAL(U)));
	};
}

template <class T>
constexpr bool is_enum = detail::IsEnum<T>::decl_value | detail::IsEnum<T>::value;
template <class T> constexpr bool is_defined_enum = detail::IsEnum<T>::value;

template <class T> using EnableIfEnum = EnableIf<is_enum<T>, NotAnEnum>;

template <class T, EnableIfEnum<T>...> T fromString(Str str) EXCEPT {
	using Strings = decltype(enumStrings(T()));
	return T(fwk::detail::parseEnum(str, Strings::offsets.data, Strings::K, true));
}

template <class T, EnableIfEnum<T>...> T tryFromString(Str str, T on_error) NOEXCEPT {
	using Strings = decltype(enumStrings(T()));
	int ret = fwk::detail::parseEnum(str, Strings::offsets.data, Strings::K, false);
	return ret == -1 ? on_error : T(ret);
}

template <class T, EnableIfEnum<T>...> Maybe<T> maybeFromString(Str str) NOEXCEPT {
	using Strings = decltype(enumStrings(T()));
	int ret = fwk::detail::parseEnum(str, Strings::offsets.data, Strings::K, false);
	if(ret == -1)
		return none;
	return T(ret);
}

template <class T, EnableIfEnum<T>...> const char *toString(T value) {
	using Strings = decltype(enumStrings(T()));
	int ivalue = (int)value;
	PASSERT(ivalue >= 0 && ivalue < Strings::K);
	return Strings::offsets.data[ivalue];
}

template <class T, EnableIfEnum<T>...> constexpr int count = decltype(enumStrings(T()))::K;

template <class Type> struct AllEnums {
	struct Iter {
		Type operator*() const { return Type(pos); }
		void operator++() { pos++; }
		bool operator==(const Iter &rhs) const { return pos == rhs.pos; }

		int pos;
	};

	Iter begin() const { return {0}; }
	Iter end() const { return {count<Type>}; }

	operator EnumFlags<Type>() const;
};

template <class T, EnableIfEnum<T>...> constexpr AllEnums<T> all;

template <class T, EnableIfEnum<T>...> T next(T value) {
	return T(int(value) == count<T> - 1 ? 0 : int(value) + 1);
}

template <class T, EnableIfEnum<T>...> T prev(T value) {
	return T(int(value) == 0 ? int(count<T> - 1) : int(value) - 1);
}

template <int offset, class T, EnableIfEnum<T>...> T next(T value) {
	static_assert(offset >= 0 && offset <= count<T>);
	int new_val = int(value) + offset;
	return T(new_val >= count<T> ? new_val - count<T> : new_val);
}
template <int offset, class T, EnableIfEnum<T>...> T prev(T value) {
	static_assert(offset >= 0 && offset <= count<T>);
	int new_val = int(value) - offset;
	return T(new_val < 0 ? new_val + count<T> : new_val);
}

template <class T> struct detail::EmptyMaybe<T, EnableIfEnum<T>> {
	static T make() { return T(255); }
	static constexpr bool valid(const T &rhs) { return int(rhs) != 255; }
};

template <class T, EnableIfEnum<T>...> TextParser &operator>>(TextParser &parser, T &value) EXCEPT {
	using Strings = decltype(enumStrings(T()));
	value = T(fwk::detail::parseEnum(parser, Strings::offsets.data, Strings::K));
	return parser;
}
}
