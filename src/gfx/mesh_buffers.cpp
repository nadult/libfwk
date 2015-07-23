/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <limits>

namespace fwk {

namespace {
	vector<float3> transformVertices(const Matrix4 &mat, vector<float3> verts) {
		for(auto &vert : verts)
			vert = mulPoint(mat, vert);
		return verts;
	}

	vector<float3> transformNormals(const Matrix4 &mat, vector<float3> normals) {
		Matrix4 nrm_mat = transpose(inverse(mat));
		for(auto &nrm : normals)
			nrm = mulNormal(nrm_mat, nrm);
		return normals;
	}
}

MeshBuffers::MeshBuffers(vector<float3> positions, vector<float3> normals,
						 vector<float2> tex_coords, vector<vector<VertexWeight>> weights,
						 vector<string> node_names)
	: positions(std::move(positions)), normals(std::move(normals)),
	  tex_coords(std::move(tex_coords)), weights(std::move(weights)),
	  node_names(std::move(node_names)) {
	// TODO: when loading from file, we want to use ASSERT, otherwise DASSERT
	// In general if data is marked as untrusted, then we have to check.

	DASSERT(tex_coords.empty() || positions.size() == tex_coords.size());
	DASSERT(normals.empty() || positions.size() == normals.size());
	DASSERT(weights.empty() || positions.size() == weights.size());

	int max_node_id = -1;
	for(const auto &vweights : weights)
		for(auto vweight : vweights)
			max_node_id = max(max_node_id, vweight.node_id);
	ASSERT(max_node_id < (int)node_names.size());
}

MeshBuffers::MeshBuffers(PVertexBuffer positions, PVertexBuffer normals, PVertexBuffer tex_coords)
	: MeshBuffers((DASSERT(positions), positions->getData<float3>()),
				  normals ? normals->getData<float3>() : vector<float3>(),
				  tex_coords ? tex_coords->getData<float2>() : vector<float2>()) {}

template <class T> static PVertexBuffer extractBuffer(PVertexArray array, int buffer_id) {
	DASSERT(array);
	const auto &sources = array->sources();
	DASSERT(buffer_id == -1 || (buffer_id >= 0 && buffer_id < (int)sources.size()));
	return buffer_id == -1 ? PVertexBuffer() : sources[buffer_id].buffer();
}

MeshBuffers::MeshBuffers(PVertexArray array, int positions_id, int normals_id, int tex_coords_id)
	: MeshBuffers(extractBuffer<float3>(array, positions_id),
				  extractBuffer<float3>(array, normals_id),
				  extractBuffer<float2>(array, tex_coords_id)) {}

static auto parseVertexWeights(const XMLNode &node) {
	vector<vector<MeshBuffers::VertexWeight>> out;

	XMLNode counts_node = node.child("vertex_weight_counts");
	XMLNode weights_node = node.child("vertex_weights");
	XMLNode node_ids_node = node.child("vertex_weight_node_ids");

	if(!counts_node && !weights_node && !node_ids_node)
		return out;

	ASSERT(counts_node && weights_node && node_ids_node);
	auto counts = counts_node.value<vector<int>>();
	auto weights = weights_node.value<vector<float>>();
	auto node_ids = node_ids_node.value<vector<int>>();

	ASSERT(weights.size() == node_ids.size());
	ASSERT(std::accumulate(begin(counts), end(counts), 0) == (int)weights.size());

	out.resize(counts.size());

	int offset = 0, max_index = 0;
	for(int n = 0; n < (int)counts.size(); n++) {
		auto &vweights = out[n];
		vweights.resize(counts[n]);
		for(int i = 0; i < counts[n]; i++) {
			max_index = max(max_index, node_ids[offset + i]);
			vweights[i] = MeshBuffers::VertexWeight(weights[offset + i], node_ids[offset + i]);
		}
		offset += counts[n];
	}

	return out;
}

MeshBuffers::MeshBuffers(const XMLNode &node)
	: MeshBuffers(node.childValue<vector<float3>>("positions", {}),
				  node.childValue<vector<float3>>("normals", {}),
				  node.childValue<vector<float2>>("tex_coords", {}), parseVertexWeights(node),
				  node.childValue<vector<string>>("node_names", {})) {}

void MeshBuffers::saveToXML(XMLNode node) const {
	using namespace xml_conversions;
	node.addChild("positions", node.own(toString(positions)));
	if(!tex_coords.empty())
		node.addChild("tex_coords", node.own(toString(tex_coords)));
	if(!normals.empty())
		node.addChild("normals", node.own(toString(normals)));

	if(!weights.empty()) {
		vector<int> counts;
		vector<float> vweights;
		vector<int> node_ids;

		for(int n = 0; n < (int)weights.size(); n++) {
			const auto &in = weights[n];
			counts.emplace_back((int)in.size());
			for(int i = 0; i < (int)in.size(); i++) {
				vweights.emplace_back(in[i].weight);
				node_ids.emplace_back(in[i].node_id);
			}
		}

		node.addChild("vertex_weight_counts", counts);
		node.addChild("vertex_weights", vweights);
		node.addChild("vertex_weight_node_ids", node_ids);
	}
	if(!node_names.empty())
		node.addChild("node_names", node_names);
}

vector<pair<Matrix4, float>> MeshBuffers::mapPose(const Pose &pose) const {
	auto mapping = pose.name_mapping(node_names);
	vector<pair<Matrix4, float>> out;
	out.reserve(node_names.size());
	for(int id : mapping) {
		float weight = id == -1 ? 0.0f : 1.0f;
		out.emplace_back(id == -1 ? Matrix4::identity() : pose.transforms[id], weight);
	}
	return out;
}

MeshBuffers MeshBuffers::remap(const vector<uint> &mapping) const {
	if(hasSkin())
		THROW("FIXME");

	vector<float3> out_positions(mapping.size());
	vector<float3> out_normals(!normals.empty() ? mapping.size() : 0);
	vector<float2> out_tex_coords(!tex_coords.empty() ? mapping.size() : 0);

	uint num_vertices = positions.size();
	DASSERT(all_of(begin(mapping), end(mapping), [=](uint idx) { return idx < num_vertices; }));

	for(int n = 0; n < (int)mapping.size(); n++)
		out_positions[n] = positions[mapping[n]];
	if(!normals.empty())
		for(int n = 0; n < (int)mapping.size(); n++)
			out_normals[n] = normals[mapping[n]];
	if(!tex_coords.empty())
		for(int n = 0; n < (int)mapping.size(); n++)
			out_tex_coords[n] = tex_coords[mapping[n]];

	return MeshBuffers{out_positions, out_normals, out_tex_coords};
}

void MeshBuffers::animatePositions(Range<float3> out_positions, const Pose &pose) const {
	DASSERT(out_positions.size() == positions.size());
	// DASSERT(m_max_node_index < (int)matrices.size());
	auto matrices = mapPose(pose);

	for(int v = 0; v < (int)size(); v++) {
		const auto &vweights = weights[v];

		float3 pos = positions[v];
		float3 out;
		float weight_sum = 0.0f;

		for(const auto &weight : vweights) {
			float cur_weight = weight.weight * matrices[weight.node_id].second;
			weight_sum += cur_weight;
			out += mulPointAffine(matrices[weight.node_id].first, pos) * cur_weight;
		}
		out_positions[v] = out / weight_sum;
	}
}

void MeshBuffers::animateNormals(Range<float3> out_normals, const Pose &pose) const {
	DASSERT(out_normals.size() == normals.size());
	// DASSERT(m_max_node_index < (int)matrices.size());
	auto matrices = mapPose(pose);

	for(int v = 0; v < (int)normals.size(); v++) {
		const auto &vweights = weights[v];

		float3 nrm = normals[v];
		float3 out;
		float weight_sum = 0.0f;

		for(const auto &weight : vweights) {
			float cur_weight = weight.weight * matrices[weight.node_id].second;
			weight_sum += cur_weight;
			out += mulNormalAffine(matrices[weight.node_id].first, nrm) * cur_weight;
		}
		out_normals[v] = out / weight_sum;
	}
}

MeshBuffers MeshBuffers::transform(const Matrix4 &matrix, MeshBuffers buffers) {
	buffers.positions = transformVertices(matrix, std::move(buffers.positions));
	buffers.normals = transformNormals(matrix, std::move(buffers.normals));
	return buffers;
}
}
