// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/vector.h"

namespace fwk {

class MeshIndices {
  public:
	using Type = PrimitiveType;
	using TriIndices = array<int, 3>;

	static bool isSupported(Type type) {
		return isOneOf(type, Type::triangles, Type::triangle_strip);
	}

	MeshIndices(vector<int> = {}, Type ptype = Type::triangles);
	MeshIndices(PBuffer, IndexType, Type ptype = Type::triangles);
	MeshIndices(vector<TriIndices>);

	static MeshIndices makeRange(int count, int first = 0, Type ptype = Type::triangles);

	const auto &data() const { return m_data; }
	Type type() const { return m_type; }

	vector<TriIndices> trisIndices() const;

	// Does not exclude degenerate triangles
	int triangleCount() const;
	int size() const { return (int)m_data.size(); }
	bool empty() const { return !m_data; }
	Pair<int> indexRange() const;

	static MeshIndices changeType(MeshIndices, Type new_type);
	static MeshIndices merge(const vector<MeshIndices> &, vector<Pair<int>> &index_ranges);
	static MeshIndices applyOffset(MeshIndices, int offset);

	vector<MeshIndices> split(int max_vertices, vector<vector<int>> &out_mappings) const;

	FWK_ORDER_BY_DECL(MeshIndices, m_data, m_type);

  private:
	vector<int> m_data;
	Type m_type;
};
}
