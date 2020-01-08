// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/qint.h"

namespace fwk {

template <int max_digits, class BigInt> void formatBig(TextFormatter &out, BigInt value) {
	char buffer[max_digits + 4];
	int pos = 0;

	bool sign = value < 0;
	if(value < 0)
		value = -value;

	while(value > 0) {
		int digit(value % 10);
		value /= 10;
		buffer[pos++] = '0' + digit;
		PASSERT(pos < max_digits);
	}

	if(pos == 0)
		buffer[pos++] = '0';
	if(sign)
		buffer[pos++] = '-';
	buffer[pos] = 0;

	std::reverse(buffer, buffer + pos);
	out << buffer;
}

TextFormatter &operator<<(TextFormatter &out, int128 value) {
	// Max digits: about 36
	formatBig<64>(out, value);
	return out;
}

TextFormatter &operator<<(TextFormatter &out, uint128 value) {
	formatBig<64>(out, value);
	return out;
}
}
