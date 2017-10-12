// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/rollback.h"

#include "fwk/hash_map.h"
#include "fwk/sys/assert.h"
#include <atomic>
#include <csetjmp>
#include <mutex>
#include <sys/types.h>

namespace fwk {
namespace detail {
	void *falloc(size_t);
	void *falloc(size_t, size_t);
	void ffree(void *);
}

namespace {
	__thread RollbackContext *t_context = nullptr;
	std::atomic<int> s_num_active = 0;

	// Note: Calls to g_alloc_func or g_free_func don't have to be protected
	// if we're not in the rollback mode, rbAlloc/rbFree are no different than falloc/ffree
	std::mutex s_alloc_funcs_mutex;
}

struct RollbackContext::Level {
	Level(int ap, BacktraceMode bm) : assert_stack_pos(ap), backtrace_mode(bm) {}

	// TODO: this can be improved
	// for example: keep a simple list of allocations and at the and
	// sort and subtract list of allocs from list of frees;
	HashMap<unsigned long long, int> allocs;
	vector<pair<AtRollback, void *>> callbacks;

	std::jmp_buf env;
	int pause_counter = 0;
	int assert_stack_pos = 0;
	BacktraceMode backtrace_mode;
};

int RollbackContext::atRollback(AtRollback func, void *arg) {
	if(auto *context = current())
		if(auto &level = context->levels.back(); level.pause_counter == 0) {
			context->is_disabled = true;
			int index = level.callbacks.size();
			level.callbacks.emplace_back(func, arg);
			context->is_disabled = false;
			return index;
		}
	return -1;
}

void RollbackContext::removeAtRollback(int index) {
	if(index == -1)
		return;

	if(auto *context = current())
		if(auto &level = context->levels.back(); level.pause_counter == 0) {
			DASSERT(level.callbacks.inRange(index));
			level.callbacks[index].first = nullptr;
		}
}

RollbackContext *RollbackContext::addContext(Maybe<BacktraceMode> bm) {
	RollbackContext *context = t_context;
	if(!context)
		t_context = context = new(fwk::detail::falloc(sizeof(RollbackContext))) RollbackContext();

	context->addLevel(bm);
	return context;
}

void RollbackContext::addLevel(Maybe<BacktraceMode> bm) {
	if(levels.empty()) {
		std::lock_guard<std::mutex> lock(s_alloc_funcs_mutex);
		if(s_num_active == 0) {
			fwk::detail::g_alloc = rbAlloc;
			fwk::detail::g_aligned_alloc = rbAlloc;
			fwk::detail::g_free = rbFree;
		}
		s_num_active++;
	}
	is_disabled = true;
	levels.emplace_back(detail::t_on_assert_count, bm ? *bm : Backtrace::t_default_mode);
	is_disabled = false;
}

void RollbackContext::dropContext() {
	dropLevel();

	// TODO: Don't reallocate all the time
	// if it's fast it will be useful in wider range of contexts
	if(levels.empty()) {
		t_context = nullptr;
		this->~RollbackContext();
		fwk::detail::ffree(this);
	}
}

void RollbackContext::dropLevel() {
	is_disabled = true;
	levels.pop_back();
	is_disabled = false;

	if(levels.empty()) {
		std::lock_guard<std::mutex> lock(s_alloc_funcs_mutex);
		s_num_active--;
		if(s_num_active == 0) {
			fwk::detail::g_alloc = fwk::detail::falloc;
			fwk::detail::g_aligned_alloc = fwk::detail::falloc;
			fwk::detail::g_free = fwk::detail::ffree;
		}
	}
}

std::jmp_buf &RollbackContext::jmpBuf() { return levels.back().env; }

void *RollbackContext::rbAlloc(size_t size) {
	auto *ptr = (char *)fwk::detail::falloc(size);

	if(auto *context = current())
		if(auto &level = context->levels.back(); level.pause_counter == 0) {
			context->is_disabled = true;
			//printf("rbAlloc %lld [%d bytes]\n", (long long)ptr, (int)size);
			level.allocs[(unsigned long long)ptr] = 0;
			context->is_disabled = false;
		}

	return ptr;
}

void *RollbackContext::rbAlloc(size_t size, size_t alignment) {
	auto *ptr = (char *)fwk::detail::falloc(size, alignment);

	if(auto *context = current())
		if(auto &level = context->levels.back(); level.pause_counter == 0) {
			context->is_disabled = true;
			//printf("rbAlloc %lld [%d bytes]\n", (long long)ptr, (int)size);
			level.allocs[(unsigned long long)ptr] = 0;
			context->is_disabled = false;
		}

	return ptr;
}

void RollbackContext::rbFree(void *ptr) {
	if(auto *context = current())
		if(auto &level = context->levels.back(); level.pause_counter == 0) {
			auto &allocs = level.allocs;
			auto it = allocs.find((unsigned long long)ptr);
			if(it == allocs.end()) {
				// TODO: how to notify user about this ?
				context->allocation_warning = true;
				//printf("rbFree:  %lld (not registered)\n", (long long)ptr);
			} else {
				//printf("rbFree:  %lld\n", (long long)ptr);
				allocs.erase(it);
			}
		}
	fwk::detail::ffree(ptr);
}

RollbackContext *RollbackContext::current() {
	if(!s_num_active)
		return nullptr;
	RollbackContext *context = t_context;
	return context && !context->is_disabled ? context : nullptr;
}

auto RollbackContext::status() -> RollbackStatus {
	RollbackStatus out{detail::t_on_assert_count, Backtrace::t_default_mode};

	if(auto *context = current()) {
		out.on_assert_top = context->levels.back().assert_stack_pos;
		out.backtrace_mode = context->levels.back().backtrace_mode;
	}

	return out;
}

void RollbackContext::pause() {
	if(auto *context = current())
		context->levels.back().pause_counter++;
}

void RollbackContext::resume() {
	if(auto *context = current())
		context->levels.back().pause_counter--;
}

bool RollbackContext::canRollback() {
	if(auto *context = current())
		return context->levels.back().pause_counter == 0;
	return false;
}

bool RollbackContext::willRollback(CSpan<const void *> pointers) {
	if(auto *context = current()) {
		const auto &level = context->levels.back();
		for(auto ptr : pointers)
			if(level.allocs.find((unsigned long long)ptr) != level.allocs.end())
				return true;
	}

	return false;
}

void RollbackContext::rollback(Error error) {
	auto *context = current();
	ASSERT(canRollback());

	auto &level = context->levels.back();
	error.validateMemory();
	context->passed_error = move(error);
	detail::t_on_assert_count = level.assert_stack_pos;

	//printf("Rollback!! freeing: %d blocks\n", level.allocs.size());
	for(auto &pair : level.callbacks)
		pair.first(pair.second);

	for(auto &pair : level.allocs) {
		fwk::detail::ffree((void *)pair.first);
		//printf("garbage:  %lld\n", pair.first);
	}

	std::longjmp(level.env, 1);

	// TODO: how to make sure that there are no pointers to objects allocated
	// in rollback mode ?
}
}
