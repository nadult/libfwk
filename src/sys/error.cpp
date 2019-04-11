// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/error.h"

#include "fwk/any.h"
#include "fwk/format.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/rollback.h"
#include <mutex>

namespace fwk {

FWK_COPYABLE_CLASS_IMPL(ErrorChunk)

void ErrorChunk::operator>>(TextFormatter &out) const {
	if(file)
		out("%:%%", file, line, message.empty() ? "\n" : ": ");
	if(!message.empty())
		out("%\n", message);
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

__thread int detail::t_num_errors = 0;

static std::mutex s_mutex;
static vector<Pair<Error, int>> s_errors;

vector<Error> getErrors() {
	vector<Error> out;
	int tid = threadId();
	s_mutex.lock();
	for(int n = 0; n < s_errors.size(); n++) {
		if(s_errors[n].second == tid) {
			out.emplace_back(move(s_errors[n].first));
			s_errors[n--] = s_errors.back();
			s_errors.pop_back();
		}
	}
	s_mutex.unlock();
	detail::t_num_errors = 0;
	return out;
}

Error getSingleError() {
	auto log = getErrors();
	DASSERT(log);
	if(log.size() == 1)
		return move(log[0]);

	vector<ErrorChunk> chunks;
	for(auto &err : log) {
		for(auto &chunk : err.chunks)
			chunks.emplace_back(chunk);
		if(err.backtrace)
			chunks.emplace_back(err.backtrace->analyze(true));
	}
	return {move(chunks), Backtrace::get()};
}

void regError(Error error, int bt_skip) {
	if(!error.backtrace)
		error.backtrace = Backtrace::get(bt_skip + 1);

	s_mutex.lock();
	s_errors.emplace_back(move(error), threadId());
	detail::t_num_errors++;
	s_mutex.unlock();
}

void regError(string text, const char *file, int line) {
	regError(Error(move(text), file, line), 1);
}
}
