// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/exception.h"

#include "fwk/sys/backtrace.h"
#include <mutex>

namespace fwk {

__thread int detail::t_num_exceptions = 0;

static std::mutex s_mutex;
static vector<Pair<Error, int>> s_errors;

vector<Error> getExceptions() {
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
	detail::t_num_exceptions = 0;
	return out;
}

Error getMergedExceptions() {
	auto list = getExceptions();
	DASSERT(list);
	if(list.size() == 1)
		return move(list[0]);

	vector<ErrorChunk> chunks;
	for(auto &err : list) {
		for(auto &chunk : err.chunks)
			chunks.emplace_back(chunk);
		if(err.backtrace)
			chunks.emplace_back(err.backtrace->analyze(true));
	}
	return {move(chunks), Backtrace::get()};
}

void raiseException(Error error, int bt_skip) {
	if(!error.backtrace)
		error.backtrace = Backtrace::get(bt_skip + 1);

	auto tid = threadId();
	detail::t_num_exceptions++;

	s_mutex.lock();
	s_errors.emplace_back(move(error), tid);
	s_mutex.unlock();
}

void clearExceptions() {
	if(detail::t_num_exceptions)
		getExceptions();
}

void printExceptions() {
	for(auto err : getExceptions())
		err.print();
}

}
