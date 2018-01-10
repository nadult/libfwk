// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/rollback.h"

namespace fwk {

FWK_COPYABLE_CLASS_IMPL(ErrorChunk)

TextFormatter &operator<<(TextFormatter &out, const ErrorChunk &chunk) {
	if(chunk.file)
		out("%:%%", chunk.file, chunk.line, chunk.message.empty() ? "\n" : ": ");
	if(!chunk.message.empty())
		out("%\n", chunk.message);
	return out;
}

Error::Error(string message, const char *file, int line) {
	chunks.emplace_back(move(message), file, line);
}

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

TextFormatter &operator<<(TextFormatter &out, const Error &error) {
	for(auto &chunk : error.chunks)
		out << chunk;
	if(error.backtrace)
		out("%\n", error.backtrace->analyze(true));
	return out;
}

void Error::print() const { fwk::print("%\n", *this); }
}
