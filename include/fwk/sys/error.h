// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys_base.h"

namespace fwk {

struct ErrorChunk {
	ErrorChunk(string message = {}, const char *file = nullptr, int line = 0)
		: message(move(message)), file(file), line(line) {}
	FWK_COPYABLE_CLASS(ErrorChunk);

	bool empty() const { return message.empty() && !file; }
	void operator>>(TextFormatter &) const;

	string message;
	const char *file;
	int line;
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
void regError(string, const char *file, int line);

template <class... T, EnableIfFormattible<T...>...>
void regError(const char *file, int line, const char *str, T &&... args) {
	regError(format(str, std::forward<T>(args)...), file, line);
}

// TODO: naming
#define REG_CHECK(expr)                                                                            \
	{                                                                                              \
		if(!(expr))                                                                                \
			regError(FWK_STRINGIZE(expr), __FILE__, __LINE__);                                     \
	}

#define REG_ERROR(format, ...)                                                                     \
	{ regError(__FILE__, __LINE__, format, __VA_ARGS__); }
}
