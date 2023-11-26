// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/mesh_indices.h"

#include <numeric>

namespace fwk {

MeshIndices::MeshIndices(vector<int> indices, Topology topology)
	: m_data(std::move(indices)), m_topology(topology) {
	DASSERT(isSupported(m_topology));
}

MeshIndices::MeshIndices(vector<TriIndices> indices) : MeshIndices(indices.reinterpret<int>()) {}

MeshIndices MeshIndices::makeRange(int count, int first, Topology ptype) {
	DASSERT(count >= 0);
	vector<int> indices(count);
	std::iota(begin(indices), end(indices), first);
	return MeshIndices(std::move(indices), ptype);
}

MeshIndices MeshIndices::merge(const vector<MeshIndices> &set, vector<Pair<int>> &index_ranges) {
	bool all_strips = std::all_of(begin(set), end(set), [](auto &inds) {
		return inds.topology() == Topology::triangle_strip;
	});
	auto topology = all_strips ? Topology::triangle_strip : Topology::triangle_list;

	vector<int> merged;
	index_ranges.clear();

	MeshIndices temp;
	for(const auto &input : set) {
		const auto &indices =
			input.topology() != topology ? (temp = changeTopology(input, topology), temp) : input;

		if(topology == Topology::triangle_strip) {
			DASSERT(indices.size() >= 3 || indices.size() == 0);
		} else if(topology == Topology::triangle_list) {
			DASSERT(indices.size() % 3 == 0);
		}

		if(topology == Topology::triangle_strip && merged && !indices.empty()) {
			int prev = merged.back(), next = indices.m_data.front();

			if(merged.size() % 2 == 1) {
				int indices[5] = {prev, prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			} else {
				int indices[4] = {prev, prev, next, next};
				merged.insert(end(merged), begin(indices), end(indices));
			}
		}

		index_ranges.emplace_back(merged.size(), indices.size());
		merged.insert(end(merged), begin(indices.m_data), end(indices.m_data));
	}

	return MeshIndices(merged, topology);
}

vector<MeshIndices> MeshIndices::split(int max_vertices, vector<vector<int>> &out_mappings) const {
	if(m_topology == Topology::triangle_strip) {
		// TODO: write me if needed
		return changeTopology(*this, Topology::triangle_list).split(max_vertices, out_mappings);
	}

	vector<MeshIndices> out;
	out_mappings.clear();

	auto range = indexRange();
	vector<int> index_map(range.second - range.first + 1, -1);

	int last_index = 0;
	while(last_index + 2 < m_data.size()) {
		int num_vertices = 0;
		vector<int> mapping, indices;

		while(last_index + 2 < m_data.size()) {
			for(int j = 0; j < 3; j++) {
				int vindex = m_data[last_index + j];
				int iindex = vindex - range.first;

				if(index_map[iindex] == -1) {
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
			index_map[idx - range.first] = -1;

		out.emplace_back(std::move(indices));
		out_mappings.emplace_back(std::move(mapping));
	}

	return out;
}

MeshIndices MeshIndices::applyOffset(MeshIndices indices, int offset) {
	if(offset > 0)
		for(auto &elem : indices.m_data)
			elem += offset;
	return indices;
}

int MeshIndices::triangleCount() const {
	int index_count = (int)m_data.size();
	return m_topology == Topology::triangle_list ? index_count / 3 : max(0, index_count - 2);
}

Pair<int> MeshIndices::indexRange() const {
	if(!m_data)
		return {};

	Pair<int> out(m_data.front(), m_data.front());
	for(const auto idx : m_data) {
		out.first = min(out.first, idx);
		out.second = max(out.second, idx);
	}
	return out;
}

vector<MeshIndices::TriIndices> MeshIndices::trisIndices() const {
	vector<TriIndices> out;
	out.reserve(triangleCount());

	if(m_topology == Topology::triangle_list) {
		for(int n = 0; n + 2 < m_data.size(); n += 3)
			out.push_back({{m_data[n + 0], m_data[n + 1], m_data[n + 2]}});

	} else {
		DASSERT(m_topology == Topology::triangle_strip);
		bool do_swap = false;

		for(int n = 2; n < m_data.size(); n++) {
			int a = m_data[n - 2];
			int b = m_data[n - 1];
			int c = m_data[n];
			if(do_swap)
				swap(b, c);
			do_swap ^= 1;

			if(a != b && b != c && a != c)
				out.push_back({{a, b, c}});
		}
	}

	return out;
}

MeshIndices MeshIndices::changeTopology(MeshIndices indices, Topology new_type) {
	if(new_type == indices.topology())
		return indices;

	if(new_type == Topology::triangle_strip)
		FATAL("Please write me");
	if(new_type == Topology::triangle_list) {
		auto tris = indices.trisIndices();
		indices.m_data.reserve(tris.size() * 3);
		indices.m_data.clear();
		for(const auto &tri : tris)
			indices.m_data.insert(end(indices.m_data), begin(tri), end(tri));
		indices.m_topology = new_type;
	}

	return indices;
}

FWK_ORDER_BY_DEF(MeshIndices, m_data, m_topology)
}
