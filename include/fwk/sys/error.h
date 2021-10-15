// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert_impl.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys_base.h"

namespace fwk {

// TODO: use source_location
struct ErrorLoc {
	const char *file = nullptr;
	int line = 0;

	FWK_ORDER_BY_DECL(ErrorLoc);
};

struct ErrorChunk {
	ErrorChunk(string message = {}) : message(move(message)) {}
	ErrorChunk(ErrorLoc loc, string message = {}) : message(move(message)), loc(loc) {}
	FWK_COPYABLE_CLASS(ErrorChunk);
	FWK_ORDER_BY_DECL(ErrorChunk);

	bool empty() const { return message.empty() && !loc.file; }
	void operator>>(TextFormatter &) const;

	string message;
	ErrorLoc loc;
};

// Creates an error with automatically formatted parameters
// Example: ERROR_EX("Invalid arguments", arg1, arg2, arg3)
#define ERROR_EX(...) _ASSERT_WITH_PARAMS(makeError, __VA_ARGS__)

// Creates an error with formatted text
// Example: ERROR("Low-case string should be passed: %", str)
#define ERROR(...) _ASSERT_FORMATTED(makeError, __VA_ARGS__)
#define FWK_ERROR(...) _ASSERT_FORMATTED(makeError, __VA_ARGS__)

struct Error {
	using Chunk = ErrorChunk;

	Error(ErrorLoc, string message);

	static Error merge(vector<Error>);

	// TODO: remove these ?
	template <class... T, EnableIfFormattible<T...>...>
	Error(ErrorLoc loc, const char *fmt, T &&... args)
		: Error(loc, format(fmt, std::forward<T>(args)...)) {}
	template <class... T, EnableIfFormattible<T...>...>
	Error(const char *fmt, T &&... args)
		: Error(ErrorLoc(), format(fmt, std::forward<T>(args)...)) {}

	Error(Chunk, Backtrace = {});
	Error(vector<Chunk>, Backtrace = {});
	Error();
	FWK_COPYABLE_CLASS(Error);
	FWK_ORDER_BY_DECL(Error);

	void operator+=(const Chunk &);

	// TODO: możliwość doklejenia też z drugiej strony?
	Error operator+(const Chunk &) const;

	Error &operator<<(Any);

	void print() const;
	bool empty() const { return !chunks && !backtrace; }

	void operator>>(TextFormatter &) const;

	vector<Chunk> chunks;
	Backtrace backtrace;
	vector<Any> values;
};
}
