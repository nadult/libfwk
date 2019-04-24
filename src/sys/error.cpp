// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/error.h"

#include "fwk/any.h"
#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include <stdarg.h>

namespace fwk {

FWK_COPYABLE_CLASS_IMPL(ErrorChunk)

void ErrorChunk::operator>>(TextFormatter &out) const {
	if(loc.file)
		out("%:%%", loc.file, loc.line, message.empty() ? "\n" : ": ");
	if(!message.empty())
		out("%\n", message);
}

Error::Error(ErrorLoc loc, string message) { chunks.emplace_back(loc, move(message)); }

Error::Error(Chunk chunk, Maybe<Backtrace> bt) : backtrace(move(bt)) {
	chunks.emplace_back(move(chunk));
}
Error::Error(vector<Chunk> chunks, Maybe<Backtrace> bt)
	: chunks(move(chunks)), backtrace(move(bt)) {}
Error::Error() = default;
FWK_COPYABLE_CLASS_IMPL(Error)

void Error::operator+=(const Chunk &chunk) { chunks.emplace_back(chunk); }

Error Error::operator+(const Chunk &chunk) const {
	auto new_chunks = chunks;
	new_chunks.emplace_back(chunk);
	return {move(new_chunks)};
}

Error &Error::operator<<(Any value) {
	values.emplace_back(move(value));
	return *this;
}

void Error::operator>>(TextFormatter &out) const {
	for(auto &chunk : chunks)
		out << chunk;
	if(backtrace)
		out("%\n", backtrace->analyze(true));
}

void Error::print() const { fwk::print("%\n", *this); }
}
