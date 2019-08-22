// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/manager.h"

#include "fwk/format.h"
#include "fwk/perf/analyzer.h"
#include "fwk/perf/exec_tree.h"
#include <pthread.h>

namespace perf {

Manager *Manager::s_instance = nullptr;
static pthread_mutex_t s_mutex;
static vector<Frame> s_new_frames;

Manager::Manager() {
	ASSERT(!s_instance && "Only single perf::Manager is allowed");
#ifndef FWK_IMGUI_DISABLED
	ASSERT(!Analyzer::instance() && "Analyzer has to be destroyed before Manager");
#endif

	s_instance = this;
	pthread_mutex_init(&s_mutex, 0);
	m_tree.emplace();
}

Manager::~Manager() {
	// TODO: make sure that all contexts are destroyed before Manager?
	s_instance = nullptr;
}

void Manager::getNewFrames() {
	PERF_SCOPE();
	pthread_mutex_lock(&s_mutex);
	for(auto &frame : s_new_frames)
		m_frames.emplace_back(move(frame));
	s_new_frames.clear();
	pthread_mutex_unlock(&s_mutex);
}

void Manager::addFrame(int frame_id, double begin, double end, CSpan<PSample> psamples,
					   double cpu_time_scale) {
	DASSERT(s_instance);
	auto &tree = s_instance->m_tree;
	auto esamples = tree->makeExecSamples(psamples);
	tree->computeGpuTimes(esamples);
	tree->scaleCpuTimes(esamples, cpu_time_scale);

	pthread_mutex_lock(&s_mutex);
	s_new_frames.emplace_back(move(esamples), begin, end, frame_id, threadId());
	pthread_mutex_unlock(&s_mutex);
}

i64 Manager::usedMemory() const {
	i64 out = 0;
	for(auto &frame : m_frames)
		out += frame.usedMemory();
	return out + m_frames.usedMemory() + m_tree->usedMemory();
}
}
