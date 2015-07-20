/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <deque>
#include "fwk_profile.h"

namespace fwk {

MeshSkin::MeshSkin() : m_max_node_index(-1) {}

MeshSkin::MeshSkin(const XMLNode &node) : m_max_node_index(-1) {
	XMLNode counts_node = node.child("counts");
	XMLNode weights_node = node.child("weights");
	XMLNode node_ids_node = node.child("node_ids");
	XMLNode node_names_node = node.child("node_names");
	if(!counts_node && !weights_node && !node_ids_node && !node_names_node)
		return;

	ASSERT(counts_node && weights_node && node_ids_node && node_names_node);
	auto counts = counts_node.value<vector<int>>();
	auto weights = weights_node.value<vector<float>>();
	auto node_ids = node_ids_node.value<vector<int>>();
	m_node_names = node_names_node.value<vector<string>>();

	ASSERT(weights.size() == node_ids.size());
	ASSERT(std::accumulate(begin(counts), end(counts), 0) == (int)weights.size());

	m_vertex_weights.resize(counts.size());
	int offset = 0, max_index = 0;
	for(int n = 0; n < (int)counts.size(); n++) {
		auto &out = m_vertex_weights[n];
		out.resize(counts[n]);
		for(int i = 0; i < counts[n]; i++) {
			max_index = max(max_index, node_ids[offset + i]);
			out[i] = VertexWeight(weights[offset + i], node_ids[offset + i]);
		}
		offset += counts[n];
	}

	ASSERT(max_index < (int)m_node_names.size());
}

void MeshSkin::saveToXML(XMLNode node) const {
	if(empty())
		return;

	vector<int> counts;
	vector<float> weights;
	vector<int> node_ids;

	for(int n = 0; n < (int)m_vertex_weights.size(); n++) {
		const auto &in = m_vertex_weights[n];
		counts.emplace_back((int)in.size());
		for(int i = 0; i < (int)in.size(); i++) {
			weights.emplace_back(in[i].weight);
			node_ids.emplace_back(in[i].node_id);
		}
	}

	using namespace xml_conversions;
	node.addChild("counts", node.own(toString(counts)));
	node.addChild("weights", node.own(toString(weights)));
	node.addChild("node_ids", node.own(toString(node_ids)));
	node.addChild("node_names", node.own(toString(m_node_names)));
}

bool MeshSkin::empty() const {
	return std::all_of(begin(m_vertex_weights), end(m_vertex_weights),
					   [](const auto &v) { return v.empty(); });
}

void MeshSkin::attach(const Model &model) {
	m_max_node_index = 0;
	m_mapping = model.findNodes(m_node_names);
	for(int n = 0; n < (int)m_mapping.size(); n++) {
		int id = m_mapping[n];
		m_max_node_index = max(m_max_node_index, id);
	}
}

void MeshSkin::animatePositions(Range<float3> positions, Range<Matrix4> matrices) const {
	DASSERT(positions.size() == m_vertex_weights.size());
	DASSERT(m_max_node_index < (int)matrices.size());
	DASSERT(!m_mapping.empty() && "Mesh skin should be attached first");

	for(int v = 0; v < (int)positions.size(); v++) {
		const auto &vweights = m_vertex_weights[v];

		float3 pos = positions[v];
		float3 out;
		float weight_sum = 0.0;

		for(const auto &weight : vweights) {
			int node_id = m_mapping[weight.node_id];
			if(node_id != -1) {
				out += mulPointAffine(matrices[node_id], pos) * weight.weight;
				weight_sum += weight.weight;
			}
		}
		positions[v] = out / weight_sum;
	}
}

void MeshSkin::animateNormals(Range<float3> normals, Range<Matrix4> matrices) const {
	DASSERT(normals.size() == m_vertex_weights.size());
	DASSERT(m_max_node_index < (int)matrices.size());
	DASSERT(!m_mapping.empty() && "Mesh skin should be attached first");

	for(int v = 0; v < (int)normals.size(); v++) {
		const auto &vweights = m_vertex_weights[v];

		float3 pos = normals[v];
		float3 out;
		for(const auto &weight : vweights) {
			int node_id = m_mapping[weight.node_id];
			out += mulNormalAffine(matrices[node_id], pos) * weight.weight;
		}
		normals[v] = out;
	}
}
}
