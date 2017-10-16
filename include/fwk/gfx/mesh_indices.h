// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

class MeshIndices {
  public:
	using Type = PrimitiveType;
	using TriIndices = array<uint, 3>;

	static bool isSupported(Type type) {
		return isOneOf(type, Type::triangles, Type::triangle_strip);
	}

	MeshIndices(vector<uint> = {}, Type ptype = Type::triangles);
	MeshIndices(PIndexBuffer, Type ptype = Type::triangles);
	MeshIndices(const vector<TriIndices> &);

	static MeshIndices makeRange(int count, uint first = 0, Type ptype = Type::triangles);

	operator const vector<uint> &() const { return m_data; }
	Type type() const { return m_type; }

	vector<TriIndices> trisIndices() const;

	// Does not exclude degenerate triangles
	int triangleCount() const;
	int size() const { return (int)m_data.size(); }
	bool empty() const { return m_data.empty(); }
	pair<uint, uint> indexRange() const;

	static MeshIndices changeType(MeshIndices, Type new_type);
	static MeshIndices merge(const vector<MeshIndices> &, vector<pair<uint, uint>> &index_ranges);
	static MeshIndices applyOffset(MeshIndices, uint offset);

	vector<MeshIndices> split(uint max_vertices, vector<vector<uint>> &out_mappings) const;

	FWK_ORDER_BY(MeshIndices, m_data, m_type);

  private:
	vector<uint> m_data;
	Type m_type;
};
}
