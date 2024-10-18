// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/thread_context.h"

#include "fwk/index_range.h"
#include "fwk/perf/manager.h"
#include "fwk/vulkan_base.h"

namespace perf {

static_assert(ThreadContext::num_swap_frames > VulkanLimits::num_swap_frames,
			  "Additional swap frame is required in perf to avoid waiting for gpu queries");

static FWK_THREAD_LOCAL ThreadContext *t_context = nullptr, *t_paused = nullptr;
static bool volatile s_gpu_paused = false;

ThreadContext *ThreadContext::current() { return t_context; }

int ThreadContext::numGpuScopes() const {
	// TODO: fixme
	return 0;
}

ThreadContext::ThreadContext(int reserve) {
	ASSERT(!t_context && !t_paused && "There can be only one perf::ThreadContext per thread");
	t_context = this;
	for(auto &frame : m_frames)
		frame.samples.reserve(reserve);
	m_stack.reserve(64);
	nextFrame();
}

ThreadContext::~ThreadContext() {
	t_context = nullptr;
	t_paused = nullptr;
}

// This is a bit slow (getTime() takes about 50ns)
static unsigned long long getTimeNs() { return u64(getTime() * 1000000000.0); }

static unsigned long long getClock() {
#ifdef FWK_PLATFORM_HTML
	return getTimeNs();
#elif defined(__clang__)
	return __builtin_readcyclecounter();
#else
	unsigned high, low;
	__asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
	return ((unsigned long long)high << 32) | low;
#endif
}

void ThreadContext::resolveGpuScopes(i64 frame_id, CSpan<Pair<uint, u64>> sample_gpu_times) {
	vector<Pair<uint, u64>> sample_stack;

	Span<PSample> samples;
	for(auto &frame : m_frames)
		if(frame.frame_id == frame_id) {
			samples = frame.samples;
			break;
		}
	if(!samples)
		return;

	for(auto [encoded_id, value] : sample_gpu_times) {
		auto [sample_id, scope_type] = GpuScope::decodeSampleId(encoded_id);

		switch(scope_type) {
		case ScopeType::enter:
			sample_stack.emplace_back(sample_id, value);
			break;

		case ScopeType::sibling: {
			auto [last_sample_id, last_value] = sample_stack.back();
			samples[last_sample_id].setValue(value - last_value);
			sample_stack.back() = {sample_id, value};
		} break;

		case ScopeType::exit:
			while(sample_stack) {
				auto [last_sample_id, last_value] = sample_stack.back();
				if(last_sample_id < sample_id)
					break;

				samples[last_sample_id].setValue(value - last_value);
				sample_stack.pop_back();
			}
			break;
		}
	}
}

void ThreadContext::nextFrame() {
	auto cur_time = getTime();
	auto cur_clock = getClock();
	auto cur_time_ns = getTimeNs();

	if(!m_is_initial) {
		auto &prev_frame = m_frames[m_swap_frame_id];
		prev_frame.begin_time = m_frame_begin;
		prev_frame.end_time = cur_time;
		prev_frame.samples.swap(m_samples);
		m_samples.clear();
		auto time_diff = prev_frame.end_time - prev_frame.begin_time;
		auto clock_diff = cur_clock - m_frame_begin_clock;
		prev_frame.cpu_time_scale = (time_diff * 1000000000.0) / double(clock_diff);
	}

	m_frame_id += m_is_initial ? 0 : 1;
	m_swap_frame_id = m_frame_id % num_swap_frames;
	m_is_initial = false;

	auto &frame = m_frames[m_swap_frame_id];
	if(frame.frame_id != -1) {
		Manager::addFrame(frame.frame_id, frame.begin_time, frame.end_time, frame.samples,
						  frame.cpu_time_scale);
		frame.samples.clear();
	}
	frame.frame_id = m_frame_id;
	m_frame_begin = cur_time;
	m_frame_begin_ns = cur_time_ns;
	m_frame_begin_clock = cur_clock;
}

bool ThreadContext::isPaused() const { return t_paused; }

void nextFrame() {
	if(t_context)
		t_context->nextFrame();
}

u64 ThreadContext::currentTimeNs() const { return getTimeNs() - m_frame_begin_ns; }

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

int ThreadContext::latestGpuScopeId() const {
	for(int n = m_stack.size() - 1; n >= 0; n--)
		if(m_stack[n].gpu_scope && !m_stack[n].pop_parent)
			return m_stack[n].id + 1;
	return -1;
}

int ThreadContext::enterGpuScope(PointId point_id) {
	if(s_gpu_paused) {
		enterScope(point_id);
		return -1;
	}

	PASSERT(point_id);
	int sample_id = m_samples.size();
	int gpu_sample_id = sample_id + 1;
	m_stack.emplace_back(sample_id, false, true);
	m_samples.emplace_back(SampleType::scope_begin, point_id, 0u);
	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);

	m_samples[sample_id].setValue(currentTimeClocks());
	return gpu_sample_id;
}

int ThreadContext::siblingGpuScope(PointId point_id) {
	siblingScope(point_id);
	if(s_gpu_paused)
		return -1;

	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);
	return m_samples.size() - 1;
}

int ThreadContext::childGpuScope(PointId point_id) {
	if(s_gpu_paused) {
		childScope(point_id);
		return -1;
	}

	PASSERT(point_id);
	auto time = currentTimeClocks();
	m_stack.emplace_back(m_samples.size(), true, true);
	m_samples.emplace_back(SampleType::scope_begin, point_id, time);
	m_samples.emplace_back(SampleType::gpu_time, point_id, 0u);

	return m_samples.size() - 1;
}

int ThreadContext::exitGpuScope(PointId point_id) {
	if(s_gpu_paused) {
		exitScope(point_id);
		return -1;
	}

	int gpu_scope_id = latestGpuScopeId();
	exitScope(point_id);
	return gpu_scope_id;
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

#define FORWARD_VOID_CALL(name)                                                                    \
	void name(PointId point_id) {                                                                  \
		if(t_context)                                                                              \
			t_context->name(point_id);                                                             \
	}

#define FORWARD_INT_CALL(name)                                                                     \
	int name(PointId point_id) { return t_context ? t_context->name(point_id) : -1; }

FORWARD_VOID_CALL(enterScope)
FORWARD_VOID_CALL(exitScope)
FORWARD_VOID_CALL(exitSingleScope)
FORWARD_VOID_CALL(childScope)
FORWARD_VOID_CALL(siblingScope)
FORWARD_INT_CALL(enterGpuScope)
FORWARD_INT_CALL(exitGpuScope)
FORWARD_VOID_CALL(exitSingleGpuScope)
FORWARD_INT_CALL(childGpuScope)
FORWARD_INT_CALL(siblingGpuScope)

#undef FORWARD_VOID_CALL
#undef FORWARD_INT_CALL

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
