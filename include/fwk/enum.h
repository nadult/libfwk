// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/str.h"

namespace fwk {

namespace detail {

	int parseEnum(Str, const char *const *strings, int count, bool check_if_invalid);
	int parseEnum(TextParser &, const char *const *strings, int count);
	unsigned long long parseFlags(TextParser &, const char *const *strings, int count);
	void formatFlags(unsigned long long, TextFormatter &, const char *const *strings, int count);
}

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

		static_assert(K <= 64, "Maximum number of enum elements is 64");

		static constexpr bool isWhiteSpace(char c) {
			return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
		}

		template <int N> struct Buffer {
			constexpr Buffer(const char *ptr) : data{} {
				for(int n = 0; n < N; n++)
					data[n] = isWhiteSpace(ptr[n]) ? 0 : ptr[n];
				data[N] = 0;
			}

			char data[N + 1];
		};

		template <int N> struct Offsets {
			constexpr Offsets(const char *ptr) : data{} {
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

		static constexpr Buffer<N> buffer = {init_str};
		static constexpr Offsets<N> offsets = {buffer.data};
	};
}

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

template <class T, EnableIfEnum<T>...> static T fromString(Str str) {
	using Strings = decltype(enumStrings(T()));
	return T(fwk::detail::parseEnum(str, Strings::offsets.data, Strings::K, true));
}

template <class T, EnableIfEnum<T>...> static Maybe<T> tryFromString(Str str) {
	using Strings = decltype(enumStrings(T()));
	int ret = fwk::detail::parseEnum(str, Strings::offsets.data, Strings::K, false);
	if(ret == -1)
		return none;
	return T(ret);
}

template <class T, EnableIfEnum<T>...> static const char *toString(T value) {
	using Strings = decltype(enumStrings(T()));
	int ivalue = (int)value;
	PASSERT(ivalue >= 0 && ivalue < Strings::K);
	return Strings::offsets.data[ivalue];
}

template <class T, EnableIfEnum<T>...> constexpr int count() {
	return decltype(enumStrings(T()))::K;
}

template <class T, EnableIfEnum<T>...> auto all() { return EnumRange<T>(0, count<T>()); }

template <class T, EnableIfEnum<T>...> T next(T value) { return T((int(value) + 1) % count<T>()); }

template <class T, EnableIfEnum<T>...> T prev(T value) {
	return T((int(value) + (count<T>() - 1)) % count<T>());
}

template <int offset, class T, EnableIfEnum<T>...> T next(T value) {
	static_assert(offset >= 0);
	return T((int(value) + offset) % count<T>());
}
template <int offset, class T, EnableIfEnum<T>...> T prev(T value) {
	static_assert(offset >= 0 && offset <= count<T>());
	return T((int(value) + (count<T>() - offset)) % count<T>());
}

template <class T> struct detail::InvalidValue<T, EnableIfEnum<T>> {
	static T make() { return T(255); }
	static constexpr bool valid(const T &rhs) { return int(rhs) != 255; }
};

template <class T, EnableIfEnum<T>...> TextParser &operator>>(TextParser &parser, T &value) {
	using Strings = decltype(enumStrings(T()));
	value = T(fwk::detail::parseEnum(parser, Strings::offsets.data, Strings::K));
	return parser;
}

template <class T> struct EnumFlags;

template <class Enum, class T> class EnumMap;
}
