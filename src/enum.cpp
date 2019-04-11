// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/enum.h"
#include "fwk/format.h"
#include "fwk/parse.h"

namespace fwk {

namespace detail {

	int parseEnum(Str str, const char *const *strings, int count, bool check_if_invalid) {
		for(int n = 0; n < count; n++)
			if(str == strings[n])
				return n;

		if(check_if_invalid) {
			auto stringized = toString(CSpan<const char *>(strings, count));
			REG_ERROR("Error when parsing enum: couldn't match \"%\" to (%)", str, stringized);
			return 0;
		}
		return -1;
	}

	int parseEnum(TextParser &parser, const char *const *strings, int count) {
		return parseEnum(parser.parseElement(), strings, count, true);
	}

	unsigned long long parseFlags(TextParser &parser, const char *const *strings, int count) {
		unsigned long long out = 0;

		Str next = parser.parseElement();
		if(next == "0")
			return 0;

		do {
			Str str = next;
			int next_or = str.find('|');
			next = next_or == -1 ? Str() : str.advance(next_or + 1);
			str = next_or == -1 ? str : str.substr(0, next_or);
			out |= 1ull << parseEnum(str, strings, count, true);
		} while(!next.empty());

		return out;
	}

	void formatFlags(unsigned long long bits, TextFormatter &out, const char *const *strings,
					 int count) {
		bool previous = false;

		for(int n : intRange(count))
			if(bits & (1ull << n)) {
				if(previous)
					out << '|';
				out << strings[n];
				previous = true;
			}
		if(!bits)
			out << '0';
	}
}
}
