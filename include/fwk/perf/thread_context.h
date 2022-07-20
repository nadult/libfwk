// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/perf_base.h"

namespace perf {

class ThreadContext {
  public:
	static constexpr int num_swap_frames = 3;

	ThreadContext(int reserve = 1024);
	~ThreadContext();

	ThreadContext(const ThreadContext &) = delete;
	void operator=(const ThreadContext &) = delete;

	int numGpuScopes() const;

	// Also sends frames to Manager
	void nextFrame();

	static ThreadContext *current();

	i64 frameId() const { return m_frame_id; }
	void resolveGpuScopes(i64 frame_id, CSpan<Pair<uint, u64>> sample_gpu_times);

	bool isPaused() const;
	bool isGpuPaused() const;

	void enterScope(PointId);
	void exitScope(PointId);
	void childScope(PointId);
	void siblingScope(PointId);
	void exitSingleScope(PointId);

	// Gpu functions return sample index into which gpu timing should be added or -1
	int enterGpuScope(PointId);
	int exitGpuScope(PointId);
	int childGpuScope(PointId);
	int siblingGpuScope(PointId);
	void exitSingleGpuScope(PointId);

	void setCounter(PointId, u64);

  private:
	void endFrame();

	u64 currentTimeNs() const;
	u64 currentTimeClocks() const;

	struct Level {
		Level(int sample_id, bool pop_parent, bool gpu_scope)
			: id(sample_id), pop_parent(pop_parent), gpu_scope(gpu_scope) {}

		u32 id : 30;
		u32 pop_parent : 1;
		u32 gpu_scope : 1;
	};

	int latestGpuScopeId() const;

	unsigned long long m_frame_begin_clock;
	unsigned long long m_frame_begin_ns;
	double m_frame_begin = -1.0;

	struct FrameData {
		vector<PSample> samples;
		double cpu_time_scale;
		double begin_time, end_time;
		i64 frame_id = -1;
	};

	// TODO: limits on depth
	vector<PSample> m_samples;
	FrameData m_frames[num_swap_frames];
	vector<Level> m_stack;
	i64 m_frame_id = 0;
	int m_swap_frame_id = 0;
	bool m_is_initial = true;
};
}
