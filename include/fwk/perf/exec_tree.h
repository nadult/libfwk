// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/perf_base.h"
#include "fwk/small_vector.h"

namespace perf {

DEFINE_ENUM(ExecNodeType, scope, gpu_time, counter);

ExecNodeType toNode(SampleType);

// Keeps information about all call stacks in the program
class ExecTree {
  public:
	using NodeType = ExecNodeType;

	static constexpr int max_size = ExecId::max;
	ExecTree(int reserve = 1024);

	struct Node {
		Node(PointId point_id, NodeType type, ExecId parent_id, int depth)
			: point_id(point_id), parent_id(parent_id), type(type), depth(depth) {}

		SmallVector<ExecId, 10> children;
		PointId point_id;
		ExecId parent_id;
		ExecId gpu_time_id;
		NodeType type;
		u8 depth;
	};

	ExecId root() const { return {}; }
	u64 hashedId(ExecId) const;

	// One point can map to different nodes (each having a different type)
	ExecId get(ExecId parent, PointId point, NodeType);

	vector<ExecId> mapSamples(CSpan<PSample>);
	vector<ESample> makeExecSamples(vector<PSample>);
	vector<PSample> makePointSamples(vector<ESample>) const;

	void scaleCpuTimes(Span<ESample>, double scale) const;
	vector<bool> emptyBranches(CSpan<i64>) const;

	// TODO: CSpan<ExecId> descendants(ExecId);

	ExecId parent(ExecId id) const { return nodes[id].parent_id; }
	CSpan<ExecId> children(ExecId id) const { return nodes[id].children; }

	const Node &get(ExecId id) const { return nodes[id]; }
	const Node &operator[](ExecId id) const { return nodes[id]; }
	int depth(ExecId id) const;

	struct ExecValue {
		i64 value = 0; // sample value or execution time
		u64 begin_time = 0;
		int num_instances = 0;
	};

	// values: sum of timings; begins: sum of scope_begin times
	void getExecValues(CSpan<ESample>, vector<ExecValue> &) const;

	int usedMemory() const;
	int size() const { return nodes.size(); }

	vector<Node> nodes;

  private:
	bool emptyBranches(ExecId, Span<bool>, CSpan<i64>) const;
	void updateDescendants();

	vector<ExecId> m_stack;
	vector<ExecId> m_descendants_data;
	vector<Span<ExecId>> m_descendants;
	bool m_descendants_outdated = false;
};
}
