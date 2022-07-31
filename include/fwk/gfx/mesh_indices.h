// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/algorithm.h"
#include "fwk/gfx/color.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

class MeshIndices {
  public:
	using Topology = VPrimitiveTopology;
	using TriIndices = array<int, 3>;

	static bool isSupported(Topology topology) {
		return isOneOf(topology, Topology::triangle_list, Topology::triangle_strip);
	}

	MeshIndices(vector<int> = {}, Topology ptype = Topology::triangle_list);
	MeshIndices(vector<TriIndices>);

	static MeshIndices makeRange(int count, int first = 0,
								 Topology ptype = Topology::triangle_list);

	const auto &data() const { return m_data; }
	Topology topology() const { return m_topology; }

	vector<TriIndices> trisIndices() const;

	// Does not exclude degenerate triangles
	int triangleCount() const;
	int size() const { return (int)m_data.size(); }
	bool empty() const { return !m_data; }
	Pair<int> indexRange() const;

	static MeshIndices changeTopology(MeshIndices, Topology new_type);
	static MeshIndices merge(const vector<MeshIndices> &, vector<Pair<int>> &index_ranges);
	static MeshIndices applyOffset(MeshIndices, int offset);

	vector<MeshIndices> split(int max_vertices, vector<vector<int>> &out_mappings) const;

	FWK_ORDER_BY_DECL(MeshIndices);

  private:
	vector<int> m_data;
	Topology m_topology;
};
}
