// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/mesh_buffers.h"

#include "fwk/gfx/pose.h"
#include "fwk/io/xml.h"
#include "fwk/math/matrix4.h"
#include <numeric>

namespace fwk {

namespace {
	vector<float3> transformVertices(const Matrix4 &mat, vector<float3> verts) {
		for(auto &vert : verts)
			vert = mulPoint(mat, vert);
		return verts;
	}

	vector<float3> transformNormals(const Matrix4 &mat, vector<float3> normals) {
		Matrix4 nrm_mat = transpose(inverseOrZero(mat));
		for(auto &nrm : normals)
			nrm = mulNormal(nrm_mat, nrm);
		return normals;
	}
}

MeshBuffers::MeshBuffers(vector<float3> positions, vector<float3> normals,
						 vector<float2> tex_coords, vector<IColor> colors,
						 vector<vector<VertexWeight>> weights, vector<string> node_names)
	: positions(std::move(positions)), normals(std::move(normals)),
	  tex_coords(std::move(tex_coords)), colors(std::move(colors)), weights(std::move(weights)),
	  node_names(std::move(node_names)) {
	// TODO: when loading from file, we want to use ASSERT, otherwise DASSERT
	// In general if data is marked as untrusted, then we have to check.

	DASSERT(!tex_coords || positions.size() == tex_coords.size());
	DASSERT(!normals || positions.size() == normals.size());
	DASSERT(!colors || positions.size() == colors.size());
	DASSERT(!weights || positions.size() == weights.size());

	int max_node_id = -1;
	for(const auto &vweights : weights)
		for(auto vweight : vweights)
			max_node_id = max(max_node_id, vweight.node_id);
	ASSERT(max_node_id < node_names.size());
}

static auto parseVertexWeights(CXmlNode node) EXCEPT {
	vector<vector<MeshBuffers::VertexWeight>> out;

	auto counts_node = node.child("vertex_weight_counts");
	auto weights_node = node.child("vertex_weights");
	auto node_ids_node = node.child("vertex_weight_node_ids");

	if(!counts_node && !weights_node && !node_ids_node)
		return out;

	ASSERT(counts_node && weights_node && node_ids_node);
	auto counts = counts_node.value<vector<int>>();
	auto weights = weights_node.value<vector<float>>();
	auto node_ids = node_ids_node.value<vector<int>>();

	ASSERT(weights.size() == node_ids.size());
	ASSERT(std::accumulate(begin(counts), end(counts), 0) == weights.size());

	out.resize(counts.size());

	int offset = 0, max_index = 0;
	for(int n = 0; n < counts.size(); n++) {
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

Ex<MeshBuffers> MeshBuffers::load(CXmlNode node) {
	return MeshBuffers(node.childValue<vector<float3>>("positions", {}),
					   node.childValue<vector<float3>>("normals", {}),
					   node.childValue<vector<float2>>("tex_coords", {}), {},
					   parseVertexWeights(node), node.childValue<vector<string>>("node_names", {}));
}

void MeshBuffers::saveToXML(XmlNode node) const {
	node.addChild("positions", positions);
	if(tex_coords)
		node.addChild("tex_coords", tex_coords);
	if(normals)
		node.addChild("normals", normals);

	if(weights) {
		vector<int> counts;
		vector<float> vweights;
		vector<int> node_ids;

		for(int n = 0; n < weights.size(); n++) {
			const auto &in = weights[n];
			counts.emplace_back(in.size());
			for(int i = 0; i < in.size(); i++) {
				vweights.emplace_back(in[i].weight);
				node_ids.emplace_back(in[i].node_id);
			}
		}

		node.addChild("vertex_weight_counts", counts);
		node.addChild("vertex_weights", vweights);
		node.addChild("vertex_weight_node_ids", node_ids);
	}
	if(node_names)
		node.addChild("node_names", node_names);
}

vector<Matrix4> MeshBuffers::mapPose(const Pose &pose) const {
	return pose.mapTransforms(pose.mapNames(node_names));
}

MeshBuffers MeshBuffers::remap(const vector<int> &mapping) const {
	if(hasSkin())
		FWK_FATAL("FIXME");

	vector<float3> out_positions(mapping.size());
	vector<float3> out_normals(normals ? mapping.size() : 0);
	vector<float2> out_tex_coords(tex_coords ? mapping.size() : 0);
	vector<IColor> out_colors(colors ? mapping.size() : 0);

	int num_vertices = positions.size();
	DASSERT(allOf(mapping, [=](int idx) { return idx < num_vertices; }));

	for(int n = 0; n < mapping.size(); n++)
		out_positions[n] = positions[mapping[n]];
	if(normals)
		for(int n = 0; n < mapping.size(); n++)
			out_normals[n] = normals[mapping[n]];
	if(tex_coords)
		for(int n = 0; n < mapping.size(); n++)
			out_tex_coords[n] = tex_coords[mapping[n]];
	if(colors)
		for(int n = 0; n < mapping.size(); n++)
			out_colors[n] = colors[mapping[n]];

	return MeshBuffers{out_positions, out_normals, out_tex_coords, out_colors};
}

vector<float3> MeshBuffers::animatePositions(CSpan<Matrix4> matrices) const {
	DASSERT(matrices.size() == node_names.size());
	vector<float3> out(positions.size());

	for(int v = 0; v < size(); v++) {
		const auto &vweights = weights[v];

		float3 blend, pos = positions[v];
		for(const auto &weight : vweights)
			blend += mulPointAffine(matrices[weight.node_id], pos) * weight.weight;
		out[v] = blend;
	}
	return out;
}

vector<float3> MeshBuffers::animateNormals(CSpan<Matrix4> matrices) const {
	DASSERT(matrices.size() == node_names.size());
	vector<float3> out(normals.size());

	for(int v = 0; v < normals.size(); v++) {
		const auto &vweights = weights[v];

		float3 blend, nrm = normals[v];
		for(const auto &weight : vweights)
			blend += mulNormalAffine(matrices[weight.node_id], nrm) * weight.weight;
		out[v] = blend;
	}
	return out;
}

MeshBuffers MeshBuffers::transform(const Matrix4 &matrix, MeshBuffers buffers) {
	buffers.positions = transformVertices(matrix, std::move(buffers.positions));
	buffers.normals = transformNormals(matrix, std::move(buffers.normals));
	return buffers;
}

FWK_ORDER_BY_DEF(MeshBuffers, positions, normals, tex_coords, colors, weights, node_names)
}
