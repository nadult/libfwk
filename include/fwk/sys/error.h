// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/backtrace.h"
#include "fwk/sys_base.h"

namespace fwk {

struct ErrorChunk {
	ErrorChunk(string message = {}, const char *file = nullptr, int line = 0)
		: message(move(message)), file(file), line(line) {}
	FWK_COPYABLE_CLASS(ErrorChunk);

	bool empty() const { return message.empty() && !file; }

	string message;
	const char *file;
	int line;

	friend TextFormatter &operator<<(TextFormatter &, const ErrorChunk &);
};

struct Error {
	using Chunk = ErrorChunk;

	Error(string message, const char *file = nullptr, int line = 0);
	Error(Chunk, Maybe<Backtrace> = none);
	Error(vector<Chunk>, Maybe<Backtrace> = none);
	Error();
	FWK_COPYABLE_CLASS(Error);

	void operator+=(const Chunk &);
	Error operator+(const Chunk &) const;

	Error &operator<<(Any);

	void print() const;
	bool empty() const { return chunks.empty() && !backtrace; }

	vector<Chunk> chunks;
	Maybe<Backtrace> backtrace;
	vector<Any> values;

	friend TextFormatter &operator<<(TextFormatter &, const Error &);
};
}
