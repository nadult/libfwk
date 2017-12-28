// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/math_base.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace fwk {

class TextParser;
struct NotParsable;

namespace detail {

	template <class T> struct IsParsable {
		template <class U>
		static auto test(U &) -> decltype(declval<TextParser &>() >> declval<U &>());
		static char test(...);
		enum { value = isSame<decltype(test(declval<T &>())), TextParser &>() };
	};

	template <class T> struct VariableParseElements {
		enum { value = false };
	};
	template <class T> struct VariableParseElements<vector<T>> {
		enum { value = true };
	};
}

template <class T> constexpr bool isParsable() { return detail::IsParsable<T>::value; }
template <class T> constexpr bool hasVariableParseElements() {
	return detail::VariableParseElements<T>::value;
};

template <class... Args> constexpr bool areParsable() {
	return Conjunction<detail::IsParsable<Args>...>::value;
}

template <class T> using EnableIfParsable = EnableIf<detail::IsParsable<T>::value, NotParsable>;
template <class... Args> using EnableIfParsableN = EnableIf<areParsable<Args...>(), NotParsable>;

// Parsing white-space separated elements
// Output generated by TextFormatter in plain mode can be parsed
// TODO: strings with whitespace in them
class TextParser {
  public:
	TextParser(ZStr str) : m_current(str) {}

	// This will work even when parser is empty
	TextParser &operator>>(Str &);
	TextParser &operator>>(string &);

	TextParser &operator>>(bool &);
	TextParser &operator>>(double &);
	TextParser &operator>>(float &);
	TextParser &operator>>(short &);
	TextParser &operator>>(unsigned short &);
	TextParser &operator>>(int &);
	TextParser &operator>>(unsigned int &);
	TextParser &operator>>(long &);
	TextParser &operator>>(unsigned long &);
	TextParser &operator>>(long long &);
	TextParser &operator>>(unsigned long long &);

	template <class TSpan, class T = SpanBase<TSpan>, EnableIfParsable<T>...>
	void parseSpan(TSpan &span) {
		for(auto &elem : span)
			*this >> elem;
	}

	void parseNotEmpty(Span<Str>);
	void parseNotEmpty(Span<string>);
	void parseInts(Span<int>);
	void parseFloats(Span<float>);
	void parseDoubles(Span<double>);

	Str current() const { return m_current; }
	void advance(int offset) { m_current = m_current.advance(offset); }

	bool empty() const { return m_current.empty(); }

	// Also skips whitespace on both sides
	Str parseElement();
	void advanceWhitespace();
	int countElements() const;

	void parseUints(Span<uint> out);
	void parseStrings(Span<string> out);

	template <class T, EnableIfParsable<T>...> T parse() {
		T value;
		*this >> value;
		return value;
	}

  private:
	ZStr m_current;
};

// To make new type parsable: simply overload operator<<:
// TextParser &operator<<(TextParser&, MyNewType &rhs);

TextParser &operator>>(TextParser &, int2 &);
TextParser &operator>>(TextParser &, int3 &);
TextParser &operator>>(TextParser &, int4 &);
TextParser &operator>>(TextParser &, double2 &);
TextParser &operator>>(TextParser &, double3 &);
TextParser &operator>>(TextParser &, double4 &);
TextParser &operator>>(TextParser &, float2 &);
TextParser &operator>>(TextParser &, float3 &);
TextParser &operator>>(TextParser &, float4 &);
TextParser &operator>>(TextParser &, DRect &);
TextParser &operator>>(TextParser &, FRect &);
TextParser &operator>>(TextParser &, IRect &);
TextParser &operator>>(TextParser &, FBox &);
TextParser &operator>>(TextParser &, IBox &);
TextParser &operator>>(TextParser &, DBox &);
TextParser &operator>>(TextParser &, Matrix4 &);
TextParser &operator>>(TextParser &, Quat &);

TextParser &operator>>(TextParser &, vector<string> &);
TextParser &operator>>(TextParser &, vector<int> &);
TextParser &operator>>(TextParser &, vector<float> &);

template <class T, EnableIfParsable<T>..., EnableIf<!hasVariableParseElements<T>()>...>
TextParser &operator>>(TextParser &parser, vector<T> &vec) {
	parser.advanceWhitespace();
	vec.clear();
	while(!parser.empty()) {
		vec.emplace_back();
		parser >> vec.back();
	}
	return parser;
}

template <class T, EnableIfEnum<T>...> TextParser &operator>>(TextParser &parser, T &value) {
	value = fromString<T>(parser.parseElement());
	return parser;
}
// TODO: parsing types from math

template <class T, EnableIf<isParsable<T>() && !isEnum<T>()>...> T fromString(ZStr str) {
	TextParser parser(str);
	T out;
	parser >> out;
	CHECK("traling data left after parsing" && parser.empty());
	return out;
}
}
