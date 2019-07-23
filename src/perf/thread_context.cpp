// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/thread_context.h"

#include "fwk/gfx/gl_query.h"
#include "fwk/index_range.h"
#include "fwk/perf/manager.h"

namespace perf {

static __thread ThreadContext *t_context = nullptr, *t_paused = nullptr;
static bool volatile s_gpu_paused = false;

int ThreadContext::numGpuQueries() {
	return t_context ? t_context->m_free_queries.size() + t_context->m_prev_queries.size() : 0;
}

ThreadContext::ThreadContext(int reserve) {
	ASSERT(!t_context && !t_paused && "There can be only one perf::ThreadContext per thread");
	t_context = this;
	m_samples.reserve(reserve);
	m_stack.reserve(64);
	nextFrame();
}

ThreadContext::~ThreadContext() {
	t_context = nullptr;
	t_paused = nullptr;
}

static unsigned long long getClock() {
#ifdef __clang__
	return __builtin_readcyclecounter();
#else
	unsigned high, low;
	__asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
	return ((unsigned long long)high << 32) | low;
#endif
}

void ThreadContext::nextFrame() {
	endFrame();
	m_frame_begin = getTime();
	m_frame_begin_clock = getClock();
}

void ThreadContext::endFrame() {
	if(m_frame_begin < 0.0)
		return;

	// TODO: record timestamps instead?
	resolveGpuQueries();

	auto time = getTime();
	auto clock = getClock();
	double scale = ((time - m_frame_begin) * 1000000000.0) / double(clock - m_frame_begin_clock);

	if(m_samples2)
		Manager::addFrame(m_frame_id++, m_times2.first, m_times2.second, m_samples2, m_scale2);
	m_times2 = {m_frame_begin, time};
	m_scale2 = scale;
	m_samples2.swap(m_samples);
	m_samples.clear();
	m_frame_begin = -1.0;
}

bool ThreadContext::isPaused() const { return t_paused; }

void ThreadContext::resolveGpuQueries() {
	DASSERT(!m_current_query && "some GlQuery is still running");
	if(!m_prev_queries && !m_prev_queries2)
		return;

	PERF_SCOPE();

	for(auto [query, sample_id] : m_prev_queries2) {
		auto &sample = m_samples2[sample_id];
		auto value = query->waitForValue();
		sample = {sample.type(), sample.id(), u64(value)};
		m_free_queries.emplace_back(query);
	}
	m_prev_queries2.swap(m_prev_queries);
	m_prev_queries.clear();
}

void ThreadContext::reserveGpuQueries(int count) {
	PASSERT_GL_THREAD();
	count -= m_free_queries.size();
	for(int n : intRange(count))
		m_free_queries.emplace_back(GlQuery::make());
	m_prev_queries.reserve(count * 2);
}

PQuery ThreadContext::getQuery() {
	if(m_free_queries) {
		auto out = m_free_queries.back();
		m_free_queries.pop_back();
		return out;
	}

	return GlQuery::make();
}

void endFrame() {
	if(t_context)
		t_context->endFrame();
}

void nextFrame() {
	if(t_context)
		t_context->nextFrame();
}

// This is a bit slow (getTime() takes about 50ns)
u64 ThreadContext::currentTimeNs() const { return u64((getTime() - m_frame_begin) * 1000000000.0); }

u64 ThreadContext::currentTimeClocks() const {
	return getClock() - m_frame_begin_clock; // TODO: handle overflow
}

void ThreadContext::enterScope(PointId point_id) {
	PASSERT(point_id);
	m_stack.emplace_back(m_samples.size(), false, false);
	m_samples.emplace_back(SampleType::scope_begin, point_id, 0u);
	m_samples.back().setValue(currentTimeClocks());
}

void ThreadContext::exitScope(PointId point_id) {
	PASSERT(point_id);
	auto time = currentTimeClocks();
	bool pop = true;
	while(pop) {
		int sample_id = m_stack.back().id;
		pop = m_stack.back().pop_parent;
		m_stack.pop_back();
		m_samples.emplace_back(SampleType::scope_end, m_samples[sample_id].id(), time);
	}
}

// Exits only a single scope (even if it was inherited)
void ThreadContext::exitSingleScope(PointId point_id) {
	m_stack.back().pop_parent = 0;
	exitScope(point_id);
}

void ThreadContext::siblingScope(PointId point_id) {
	PASSERT(point_id);
	auto time = currentTimeClocks();
	auto sibling_id = m_samples[m_stack.back().id].id();
	m_samples.emplace_back(SampleType::scope_end, sibling_id, time);
	m_stack.back().id = m_samples.size();
	m_samples.emplace_back(SampleType::scope_begin, point_id, time);
}

void ThreadContext::childScope(PointId point_id) {
	PASSERT(point_id);
	auto time = currentTimeClocks();
	m_stack.emplace_back(m_samples.size(), true, false);
	m_samples.emplace_back(SampleType::scope_begin, point_id, time);
}

int ThreadContext::latestGlQueryId() const {
	for(int n = m_stack.size() - 1; n >= 0; n--)
		if(m_stack[n].gpu_scope)
			return m_stack[n].id + 1;
	return -1;
}

void ThreadContext::enterGpuScope(PointId point_id) {
	if(s_gpu_paused)
		return enterScope(point_id);

	PASSERT(point_id);

	auto query = getQuery();
	if(m_current_query) {
		m_current_query->end();
		m_prev_queries.emplace_back(m_current_query, m_current_query_id);
	}
	m_current_query = query;

	int sample_id = m_samples.size();
	m_current_query_id = sample_id + 1;
	m_stack.emplace_back(sample_id, false, true);
	m_samples.emplace_back(SampleType::scope_begin, point_id, 0u);
	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);

	m_samples[sample_id].setValue(currentTimeClocks());
	query->begin(GlQueryType::time_elapsed);
}

void ThreadContext::siblingGpuScope(PointId point_id) {
	siblingScope(point_id);
	if(s_gpu_paused)
		return;

	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);

	PASSERT(m_current_query);
	m_current_query->end();
	m_prev_queries.emplace_back(m_current_query, m_current_query_id);
	m_current_query = getQuery();
	m_current_query->begin(GlQueryType::time_elapsed);
	m_current_query_id = m_samples.size() - 1;
}

void ThreadContext::childGpuScope(PointId point_id) {
	if(s_gpu_paused)
		return childScope(point_id);

	PASSERT(point_id);
	auto time = currentTimeClocks();
	m_stack.emplace_back(m_samples.size(), true, true);
	m_samples.emplace_back(SampleType::scope_begin, point_id, time);
	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);

	PASSERT(m_current_query);
	m_current_query->end();
	m_prev_queries.emplace_back(m_current_query, m_current_query_id);
	m_current_query = getQuery();
	m_current_query->begin(GlQueryType::time_elapsed);
	m_current_query_id = m_samples.size() - 1;
}

void ThreadContext::exitGpuScope(PointId point_id) {
	if(s_gpu_paused)
		return exitScope(point_id);

	PASSERT(m_current_query);
	m_current_query->end();
	m_prev_queries.emplace_back(m_current_query, m_current_query_id);
	m_current_query.reset();

	exitScope(point_id);

	m_current_query_id = latestGlQueryId();
	if(m_current_query_id != -1) {
		m_current_query = getQuery();
		m_current_query->begin(GlQueryType::time_elapsed);
	}
}

// Exits only a single scope (even if it was inherited)
void ThreadContext::exitSingleGpuScope(PointId point_id) {
	if(s_gpu_paused)
		return exitSingleScope(point_id);

	m_stack.back().pop_parent = 0;
	exitGpuScope(point_id);
}

void ThreadContext::setCounter(PointId point_id, u64 value) {
	m_samples.emplace_back(SampleType::counter, point_id, u64(value));
}

#define FORWARD_CALL(name)                                                                         \
	void name(PointId point_id) {                                                                  \
		if(t_context)                                                                              \
			return t_context->name(point_id);                                                      \
	}

FORWARD_CALL(enterScope)
FORWARD_CALL(exitScope)
FORWARD_CALL(exitSingleScope)
FORWARD_CALL(childScope)
FORWARD_CALL(siblingScope)
FORWARD_CALL(enterGpuScope)
FORWARD_CALL(exitGpuScope)
FORWARD_CALL(exitSingleGpuScope)
FORWARD_CALL(childGpuScope)
FORWARD_CALL(siblingGpuScope)

#undef FORWARD_CALL

void setCounter(PointId point_id, u64 value) {
	if(t_context)
		return t_context->setCounter(point_id, value);
}

void pause() {
	if(t_context || t_paused) {
		// TODO: debug check that scope begin & ends match
		PASSERT(t_context);
		swap(t_context, t_paused);
	}
}
void resume() {
	if(t_context || t_paused) {
		PASSERT(t_paused);
		swap(t_context, t_paused);
	}
}

void pauseGpu() {
	if(t_context) {
		PASSERT(!s_gpu_paused);
		s_gpu_paused = true;
	}
}
void resumeGpu() {
	if(t_context) {
		PASSERT(s_gpu_paused);
		s_gpu_paused = false;
	}
}
}
