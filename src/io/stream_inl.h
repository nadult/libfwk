// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/stream.h"

#include "fwk/format.h"

namespace fwk {

template <class Base> void TStream<Base>::saveSize(i64 size) {
	DASSERT(size >= 0);
	if(size < 254)
		*this << u8(size);
	else if(size <= UINT_MAX)
		pack(u8(254), u32(size));
	else
		pack(u8(255), size);
}

template <class Base> void TStream<Base>::saveString(CSpan<char> str) {
	saveSize(str.size());
	saveData(str);
}

template <class Base> void TStream<Base>::saveVector(CSpan<char> vec, int element_size) {
	DASSERT(vec.size() % element_size == 0);
	saveSize(vec.size() / element_size);
	saveData(vec);
}

template <class Base> i64 TStream<Base>::loadSize() {
	if(!isValid())
		return 0;

	i64 out = 0;
	{
		u8 small;
		*this >> small;
		if(small == 254) {
			u32 len32;
			*this >> len32;
			out = len32;
		} else if(small == 255) {
			*this >> out;
		} else {
			out = small;
		}
	}

	if(out < 0) {
		this->raise(format("Invalid length: %", out));
		return 0;
	}

	return out;
}

template <class Base> string TStream<Base>::loadString(int max_size) {
	auto size = loadSize();
	if(size > max_size) {
		this->raise(format("String too big: % > %", size, max_size));
		return {};
	}

	string out(size, ' ');
	loadData(out);
	if(!isValid())
		out = {};
	return out;
}

template <class Base> int TStream<Base>::loadString(Span<char> str) {
	PASSERT(str.size() >= 1);
	auto size = loadSize();
	int max_size = str.size() - 1;
	if(size > max_size) {
		this->raise(format("String too big: % > %", size, max_size));
		str[0] = 0;
		return 0;
	}

	loadData(span(str.data(), size));
	if(!isValid())
		size = 0;
	str[size] = 0;
	return size;
}
	
template <class Base>
TStream<Base> &TStream<Base>::operator<<(Str str) {
	saveString(str);
	return *this;
}

template <class Base>
TStream<Base> &TStream<Base>::operator>>(string &str) {
	loadString(str);
	return *this;
}
}
