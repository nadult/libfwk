// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_PARSE_H
#define FWK_PARSE_H

#include "fwk/math_base.h"
#include "fwk/sys_base.h"

namespace fwk {

// Parsing white-space separated elements
// Output generated by TextFormatter in plain mode can be parsed
class TextParser {
  public:
	TextParser(const char *input) : m_current(input) { DASSERT(m_current); }

	bool parseBool();
	int parseInt();
	long parseLong();
	long long parseLongLong();
	float parseFloat();
	double parseDouble();
	unsigned int parseUInt();
	unsigned long parseULong();
	unsigned long long parseULongLong();
	string parseString();

	void parseInts(Span<int> out);
	void parseFloats(Span<float> out);
	void parseDoubles(Span<double> out);
	void parseUints(Span<uint> out);
	void parseStrings(Span<string> out);

	bool hasAnythingLeft();
	int countElements() const;
	bool isFinished() const { return !*m_current; }

  private:
	const char *m_current;
};

namespace detail {

	template <class T> T fromString(TextParser &) {
		static_assert(sizeof(T) < 0, "xml_conversions::fromString unimplemented for given type");
	}

	template <> string fromString<string>(TextParser &);
	template <> bool fromString<bool>(TextParser &);
	template <> int fromString<int>(TextParser &);
	template <> long fromString<long>(TextParser &);
	template <> long long fromString<long long>(TextParser &);
	template <> unsigned int fromString<unsigned int>(TextParser &);
	template <> unsigned long fromString<unsigned long>(TextParser &);
	template <> unsigned long long fromString<unsigned long long>(TextParser &);
	template <> double fromString<double>(TextParser &);
	template <> float fromString<float>(TextParser &);

	template <> int2 fromString<int2>(TextParser &);
	template <> int3 fromString<int3>(TextParser &);
	template <> int4 fromString<int4>(TextParser &);
	template <> double2 fromString<double2>(TextParser &);
	template <> double3 fromString<double3>(TextParser &);
	template <> double4 fromString<double4>(TextParser &);
	template <> float2 fromString<float2>(TextParser &);
	template <> float3 fromString<float3>(TextParser &);
	template <> float4 fromString<float4>(TextParser &);
	template <> DRect fromString<DRect>(TextParser &);
	template <> FRect fromString<FRect>(TextParser &);
	template <> IRect fromString<IRect>(TextParser &);
	template <> FBox fromString<FBox>(TextParser &);
	template <> IBox fromString<IBox>(TextParser &);
	template <> DBox fromString<DBox>(TextParser &);
	template <> Matrix4 fromString<Matrix4>(TextParser &);
	template <> Quat fromString<Quat>(TextParser &);

	template <class T> vector<T> vectorFromString(TextParser &parser) {
		vector<T> out;
		while(parser.hasAnythingLeft())
			out.emplace_back(detail::fromString<T>(parser));
		return out;
	}

	template <> vector<float> vectorFromString<float>(TextParser &);
	template <> vector<double> vectorFromString<double>(TextParser &);
	template <> vector<int> vectorFromString<int>(TextParser &);
	template <> vector<uint> vectorFromString<uint>(TextParser &);
	template <> vector<string> vectorFromString<string>(TextParser &);

	template <class T> struct SelectParser {
		static T parse(TextParser &parser) { return detail::fromString<T>(parser); }
	};

	template <class T> struct SelectParser<vector<T>> {
		static vector<T> parse(TextParser &parser) { return detail::vectorFromString<T>(parser); }
	};
}

template <class T> T fromString(TextParser &parser) {
	return detail::SelectParser<T>::parse(parser);
}

template <class T, EnableIf<!isEnum<T>()>...> T fromString(const char *input) {
	TextParser parser(input);
	return detail::SelectParser<T>::parse(parser);
}

template <class T, EnableIf<!isEnum<T>()>...> T fromString(const string &input) {
	TextParser parser(input.c_str());
	return detail::SelectParser<T>::parse(parser);
}
}

#endif
