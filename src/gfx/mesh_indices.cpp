/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <limits>
#include <numeric>

namespace fwk {

MeshIndices::MeshIndices(vector<uint> indices, Type type)
	: m_data(std::move(indices)), m_type(type) {
	DASSERT(isSupported(m_type));
}

MeshIndices::MeshIndices(Range<uint> indices, Type type)
	: MeshIndices(vector<uint>(begin(indices), end(indices)), type) {}
MeshIndices::MeshIndices(PIndexBuffer indices, Type type) : MeshIndices(indices->getData(), type) {}

MeshIndices MeshIndices::makeRange(int count, uint first, Type ptype) {
	DASSERT(count >= 0);
	vector<uint> indices(count);
	std::iota(begin(indices), end(indices), first);
	return MeshIndices(std::move(indices), ptype);
}

MeshIndices MeshIndices::merge(const vector<MeshIndices> &set,
							   vector<pair<uint, uint>> &index_ranges) {
	auto type = std::all_of(begin(set), end(set), [](auto &inds) {
		return inds.type() == Type::triangle_strip;
	}) ? Type::triangle_strip : Type::triangles;

	vector<uint> merged;
	index_ranges.clear();

	MeshIndices temp;
	for(const auto &input : set) {
		const auto &indices = input.type() != type ? (temp = changeType(input, type), temp) : input;

		if(type == Type::triangle_strip) {
			DASSERT(indices.size() >= 3 || indices.size() == 0);
		} else if(type == Type::triangles) { DASSERT(indices.size() % 3 == 0); }

		if(type == Type::triangle_strip && !merged.empty() && !indices.empty()) {
			uint prev = merged.back(), next = indices.m_data.front();

			if(merged.size() % 2 == 1) {
				uint indices[5] = {prev, prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			} else {
				uint indices[4] = {prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			}
		}

		index_ranges.emplace_back(merged.size(), indices.size());
		merged.insert(end(merged), begin(indices.m_data), end(indices.m_data));
	}

	return MeshIndices(merged, type);
}

vector<MeshIndices> MeshIndices::split(uint max_vertices,
									   vector<vector<uint>> &out_mappings) const {
	if(m_type == Type::triangle_strip) {
		// TODO: write me if needed
		return changeType(*this, Type::triangles).split(max_vertices, out_mappings);
	}

	vector<MeshIndices> out;
	out_mappings.clear();

	auto range = indexRange();
	vector<uint> index_map(range.second - range.first + 1, ~0u);

	uint last_index = 0;
	while(last_index + 2 < m_data.size()) {
		uint num_vertices = 0;
		vector<uint> mapping, indices;

		while(last_index + 2 < m_data.size()) {
			for(int j = 0; j < 3; j++) {
				uint vindex = m_data[last_index + j];
				uint iindex = vindex - range.first;

				if(index_map[iindex] == ~0u) {
					index_map[iindex] = num_vertices++;
					mapping.emplace_back(vindex);
				}
				indices.emplace_back(index_map[iindex]);
			}

			if(num_vertices + 3 > max_vertices)
				break;
			last_index += 3;
		}

		for(auto idx : mapping)
			index_map[idx - range.first] = ~0u;

		out.emplace_back(std::move(indices));
		out_mappings.emplace_back(std::move(mapping));
	}

	return out;
}

MeshIndices MeshIndices::applyOffset(MeshIndices indices, uint offset) {
	if(offset > 0)
		for(auto &elem : indices.m_data)
			elem += offset;
	return indices;
}

int MeshIndices::triangleCount() const {
	int index_count = (int)m_data.size();
	return m_type == Type::triangles ? index_count / 3 : max(0, index_count - 2);
}

pair<uint, uint> MeshIndices::indexRange() const {
	if(m_data.empty())
		return make_pair(0, 0);

	pair<uint, uint> out(m_data.front(), m_data.front());
	for(const auto idx : m_data) {
		out.first = min(out.first, idx);
		out.second = max(out.second, idx);
	}
	return out;
}

vector<MeshIndices::TriIndices> MeshIndices::trisIndices() const {
	vector<TriIndices> out;
	out.reserve(triangleCount());

	if(m_type == Type::triangles) {
		for(uint n = 0; n + 2 < m_data.size(); n += 3)
			out.push_back({{m_data[n + 0], m_data[n + 1], m_data[n + 2]}});

	} else {
		DASSERT(m_type == Type::triangle_strip);
		bool do_swap = false;

		for(uint n = 2; n < m_data.size(); n++) {
			uint a = m_data[n - 2];
			uint b = m_data[n - 1];
			uint c = m_data[n];
			if(do_swap)
				swap(b, c);
			do_swap ^= 1;

			if(a != b && b != c && a != c)
				out.push_back({{a, b, c}});
		}
	}

	return out;
}

MeshIndices MeshIndices::changeType(MeshIndices indices, Type new_type) {
	if(new_type == indices.type())
		return indices;

	if(new_type == Type::triangle_strip)
		THROW("Please write me");
	if(new_type == Type::triangles) {
		auto tris = indices.trisIndices();
		indices.m_data.reserve(tris.size() * 3);
		indices.m_data.clear();
		for(const auto &tri : tris)
			indices.m_data.insert(end(indices.m_data), begin(tri), end(tri));
		indices.m_type = new_type;
	}

	return indices;
}

/*
void Mesh::genAdjacency() {
	struct TVec : public float3 {
		TVec(const float3 &vec) : float3(vec) {}
		TVec() {}

		bool operator<(const TVec &rhs) const {
			return v[0] == rhs[0] ? v[1] == rhs[1] ? v[2] < rhs[2] : v[1] < rhs[1] : v[0] <
rhs[0];
		}
	};

	struct Edge {
		Edge(const TVec &a, const TVec &b, int faceIdx) : faceIdx(faceIdx) {
			v[0] = a;
			v[1] = b;
			if(v[0] < v[1])
				swap(v[0], v[1]);
		}

		bool operator<(const Edge &rhs) const {
			return v[0] == rhs.v[0] ? v[1] < rhs.v[1] : v[0] < rhs.v[0];
		}

		bool operator==(const Edge &rhs) const { return v[0] == rhs.v[0] && v[1] == rhs.v[1]; }

		TVec v[2];
		int faceIdx;
	};

	vector<Edge> edges;
	for(int f = 0; f < faceCount(); f++) {
		int idx[3];
		getFace(f, idx[0], idx[1], idx[2]);
		edges.push_back(Edge(m_positions[idx[0]], m_positions[idx[1]], f));
		edges.push_back(Edge(m_positions[idx[1]], m_positions[idx[2]], f));
		edges.push_back(Edge(m_positions[idx[2]], m_positions[idx[0]], f));
	}
	std::sort(edges.begin(), edges.end());

	m_neighbours.resize(faceCount() * 3);
	for(int f = 0; f < faceCount(); f++) {
		int idx[3];
		getFace(f, idx[0], idx[1], idx[2]);

		for(int i = 0; i < 3; i++) {
			Edge edge(m_positions[idx[i]], m_positions[idx[(i + 1) % 3]], ~0u);
			auto edg = lower_bound(edges.begin(), edges.end(), edge);

			int neighbour = ~0u;
			while(edg != edges.end() && *edg == edge) {
				if(edg->faceIdx != f) {
					neighbour = edg->faceIdx;
					break;
				}
				++edg;
			}
			m_neighbours[f * 3 + i] = neighbour;
		}
	}
}

int Mesh::faceCount() const {
	return m_primitive_type == PrimitiveType::triangle_strip ? m_positions.size() - 2
															 : m_positions.size() / 3;
}

void Mesh::getFace(int face, int &i1, int &i2, int &i3) const {
	if(m_primitive_type == PrimitiveType::triangle_strip) {
		i1 = face + 0;
		i2 = face + 1;
		i3 = face + 2;
	} else {
		i1 = face * 3 + 0;
		i2 = face * 3 + 1;
		i3 = face * 3 + 2;
	}
}*/
}
