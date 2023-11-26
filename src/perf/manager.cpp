// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/manager.h"

#include "fwk/format.h"
#include "fwk/perf/analyzer.h"
#include "fwk/perf/exec_tree.h"
#include "fwk/sys/thread.h"

namespace perf {

Manager *Manager::s_instance = nullptr;
static Mutex s_mutex;
static vector<Frame> s_new_frames;

Manager::Manager() {
	ASSERT(!s_instance && "Only single perf::Manager is allowed");
#ifndef FWK_IMGUI_DISABLED
	ASSERT(!Analyzer::instance() && "Analyzer has to be destroyed before Manager");
#endif

	s_instance = this;
	m_tree.emplace();
}

Manager::~Manager() {
	// TODO: make sure that all contexts are destroyed before Manager?
	s_instance = nullptr;
}

void Manager::getNewFrames() {
	MutexLocker lock(s_mutex);
	for(auto &frame : s_new_frames)
		m_frames.emplace_back(std::move(frame));
	s_new_frames.clear();
}

void Manager::addFrame(int frame_id, double begin, double end, CSpan<PSample> psamples,
					   double cpu_time_scale) {
	DASSERT(s_instance);
	auto &tree = s_instance->m_tree;
	auto esamples = tree->makeExecSamples(psamples);
	tree->scaleCpuTimes(esamples, cpu_time_scale);

	MutexLocker lock(s_mutex);
	s_new_frames.emplace_back(std::move(esamples), begin, end, frame_id, threadId());
}

i64 Manager::usedMemory() const {
	i64 out = 0;
	for(auto &frame : m_frames)
		out += frame.usedMemory();
	return out + m_frames.usedMemory() + m_tree->usedMemory();
}

void Manager::limitMemory(i64 max_mem, int min_frames) {
	if(!m_frames)
		return;

	min_frames = min(min_frames, m_frames.size());
	i64 total_mem = 0;
	int first_frame = m_frames.size() - 1;
	while(first_frame >= 0 && total_mem < max_mem)
		total_mem += m_frames[first_frame--].usedMemory();
	first_frame = min(m_frames.size() - min_frames, max(first_frame, 0));
	m_frames.erase(m_frames.begin(), &m_frames[first_frame]);
}
}
