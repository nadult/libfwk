// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/exception.h"

#include "fwk/sys/backtrace.h"
#include "fwk/sys/thread.h"

namespace fwk {

FWK_THREAD_LOCAL bool detail::t_exception_raised = false;
FWK_THREAD_LOCAL int detail::t_quiet_exceptions = 0;

static Mutex s_mutex;
static vector<Pair<Error, int>> s_errors;

vector<Error> getExceptions() {
	vector<Error> out;

	if(!detail::t_exception_raised)
		return out;

	int tid = threadId();
	s_mutex.lock();
	for(int n = 0; n < s_errors.size(); n++) {
		if(s_errors[n].second == tid) {
			out.emplace_back(std::move(s_errors[n].first));
			s_errors[n--] = s_errors.back();
			s_errors.pop_back();
		}
	}
	s_mutex.unlock();
	detail::t_exception_raised = false;

	if(detail::t_quiet_exceptions || !out)
		out.emplace_back(ErrorChunk("Exceptions in quiet mode: no details provided"));

	return out;
}

Error getMergedExceptions() { return Error::merge(getExceptions()); }

void raiseException(Error error, int bt_skip) {
	detail::t_exception_raised = true;
	if(detail::t_quiet_exceptions)
		return;

	if(!error.backtrace && Backtrace::t_is_enabled && Backtrace::t_on_except_enabled)
		error.backtrace = Backtrace::get(bt_skip + 1, nullptr, true);

	auto tid = threadId();
	MutexLocker lock(s_mutex);
	s_errors.emplace_back(std::move(error), tid);
}

void clearExceptions() {
	if(detail::t_exception_raised) {
		detail::t_exception_raised = false;

		int tid = threadId();
		MutexLocker lock(s_mutex);
		for(int n = 0; n < s_errors.size(); n++) {
			if(s_errors[n].second == tid) {
				s_errors[n--] = s_errors.back();
				s_errors.pop_back();
			}
		}
	}
}

void printExceptions() {
	for(auto err : getExceptions())
		err.print();
}
}
