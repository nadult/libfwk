// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/perf_base.h"
#include "fwk/small_vector.h"

namespace perf {

class Manager {
  public:
	// TODO: limit nr of frames
	Manager();
	~Manager();

	Manager(const Manager &) = delete;
	void operator=(const Manager &) = delete;

	static Manager *instance() { return s_instance; }

	// Thread-safe
	static void addFrame(int frame_id, double begin, double end, CSpan<PSample>,
						 double cpu_time_scale);
	void getNewFrames();

	ExecTree &execTree() { return *m_tree.get(); }
	CSpan<Frame> frames() const { return m_frames; }

	i64 usedMemory() const;
	void limitMemory(i64 max_mem, int min_frames = 120);

  private:
	vector<Frame> m_frames;
	Dynamic<ExecTree> m_tree;
	static Manager *s_instance;
};
}
