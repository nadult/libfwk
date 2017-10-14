// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include "fwk/sys/backtrace.h"

namespace fwk {

// Be careful when passing these through rollback mechanism
// You have to make sure that they are created when rollback mechanism is disabled
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

	Error(Chunk, Maybe<Backtrace> = none);
	Error(vector<Chunk>, Maybe<Backtrace> = none);
	Error();
	FWK_MOVABLE_CLASS(Error);

	void operator+=(const Chunk &);
	Error operator+(const Chunk &) const;

	void print() const;
	void validateMemory();
	bool empty() const { return chunks.empty() && !backtrace; }

	vector<Chunk> chunks;
	Maybe<Backtrace> backtrace;

	friend TextFormatter &operator<<(TextFormatter &, const Error &);
};
}
