// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys_base.h"

namespace fwk {

// TODO: jak zbierać różne błędy do kupy ?

struct ErrorLoc {
	const char *file = nullptr;
	int line = 0;
};

struct ErrorChunk {
	ErrorChunk(string message = {}) : message(move(message)) {}
	ErrorChunk(ErrorLoc loc, string message = {}) : message(move(message)), loc(loc) {}
	FWK_COPYABLE_CLASS(ErrorChunk);

	bool empty() const { return message.empty() && !loc.file; }
	void operator>>(TextFormatter &) const;

	string message;
	ErrorLoc loc;
};

struct Error {
	using Chunk = ErrorChunk;

	Error(ErrorLoc, string message);

	template <class... T, EnableIfFormattible<T...>...>
	Error(ErrorLoc loc, const char *fmt, T &&... args)
		: Error(loc, format(fmt, std::forward<T>(args)...)) {}
	template <class... T, EnableIfFormattible<T...>...>
	Error(const char *fmt, T &&... args)
		: Error(ErrorLoc(), format(fmt, std::forward<T>(args)...)) {}

	Error(Chunk, Maybe<Backtrace> = none);
	Error(vector<Chunk>, Maybe<Backtrace> = none);
	Error();
	FWK_COPYABLE_CLASS(Error);

	void operator+=(const Chunk &);

	// TODO: możliwość doklejenia też z drugiej strony?
	Error operator+(const Chunk &) const;

	Error &operator<<(Any);

	void print() const;
	bool empty() const { return !chunks && !backtrace; }

	void operator>>(TextFormatter &) const;

	vector<Chunk> chunks;
	Maybe<Backtrace> backtrace;
	vector<Any> values;
};

namespace detail {
	extern __thread int t_num_errors;
}

// Simple per-thread error register
// TODO: explain

inline bool anyErrors() { return detail::t_num_errors > 0; }
inline bool noErrors() { return detail::t_num_errors == 0; }
inline int numErrors() { return detail::t_num_errors; }

vector<Error> getErrors();
Error getSingleError();
void regError(Error, int bt_skip = 0);

template <class... T, EnableIfFormattible<T...>...>
void regError(ErrorLoc loc, const char *fmt, T &&... args) {
	regError(loc, format(fmt, std::forward<T>(args)...));
}

#define ERROR(...) Error({__FILE__, __LINE__}, __VA_ARGS__)

// TODO: naming
#define REG_CHECK(expr)                                                                            \
	{                                                                                              \
		if(!(expr))                                                                                \
			regError(FWK_STRINGIZE(expr), __FILE__, __LINE__);                                     \
	}

#define REG_ERROR(...)                                                                             \
	{ regError(Error({__FILE__, __LINE__}, __VA_ARGS__)); }
}
