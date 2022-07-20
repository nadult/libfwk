// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/perf_base.h"

namespace perf {

class ThreadContext {
  public:
	ThreadContext(int reserve = 1024);
	~ThreadContext();

	ThreadContext(const ThreadContext &) = delete;
	void operator=(const ThreadContext &) = delete;

	// Sends frame to manager; resolves queries
	void nextFrame();
	void endFrame();

	bool isPaused() const;
	bool isGpuPaused() const;

	void enterScope(PointId);
	void exitScope(PointId);
	void childScope(PointId);
	void siblingScope(PointId);
	void exitSingleScope(PointId);

	void enterGpuScope(PointId);
	void exitGpuScope(PointId);
	void childGpuScope(PointId);
	void siblingGpuScope(PointId);
	void exitSingleGpuScope(PointId);

	void setCounter(PointId, u64);

	void reserveGpuQueries(int count);
	void resolveGpuQueries();

	static int numGpuQueries();

  private:
	PQuery getQuery();
	u64 currentTimeNs() const;
	u64 currentTimeClocks() const;

	struct Level {
		Level(int sample_id, bool pop_parent, bool gpu_scope)
			: id(sample_id), pop_parent(pop_parent), gpu_scope(gpu_scope) {}

		u32 id : 30;
		u32 pop_parent : 1;
		u32 gpu_scope : 1;
	};

	int latestGlQueryId() const;

	// TODO: limits on depth
	unsigned long long m_frame_begin_clock;
	unsigned long long m_frame_begin_ns;
	double m_frame_begin = -1.0;
	// times2, scale2, samples2 and prev_queries2 are used for double buffering
	// it allows resolving gpu queries in the next frame
	double m_scale2;
	pair<double, double> m_times2;
	vector<PSample> m_samples, m_samples2;
	vector<PQuery> m_free_queries;
	vector<pair<PQuery, int>> m_prev_queries, m_prev_queries2;
	vector<Level> m_stack;
	PQuery m_current_query;
	int m_current_query_id;
	int m_frame_id = 0;
};
}
