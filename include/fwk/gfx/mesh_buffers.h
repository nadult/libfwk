// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/gl_ref.h"

namespace fwk {

struct MeshBuffers {
  public:
	struct VertexWeight {
		VertexWeight(float weight = 0.0f, int node_id = 0) : weight(weight), node_id(node_id) {}

		FWK_ORDER_BY(VertexWeight, weight, node_id);

		float weight;
		int node_id;
	};

	MeshBuffers() = default;
	MeshBuffers(vector<float3> positions, vector<float3> normals = {},
				vector<float2> tex_coords = {}, vector<IColor> colors = {},
				vector<vector<VertexWeight>> weights = {}, vector<string> node_names = {});
	MeshBuffers(PBuffer positions, PBuffer normals = {}, PBuffer tex_coords = {},
				PBuffer colors = {});
	MeshBuffers(PVertexArray, int positions_id, int normals_id = -1, int tex_coords_id = -1,
				int color_id = -1);

	static Ex<MeshBuffers> load(CXmlNode);
	void saveToXML(XmlNode) const;

	vector<float3> animatePositions(CSpan<Matrix4>) const;
	vector<float3> animateNormals(CSpan<Matrix4>) const;

	int size() const { return (int)positions.size(); }
	bool hasSkin() const { return weights && node_names; }

	MeshBuffers remap(const vector<int> &mapping) const;

	// Pose must have configuration for each of the nodes in node_names
	vector<Matrix4> mapPose(PPose skinning_pose) const;

	static MeshBuffers transform(const Matrix4 &, MeshBuffers);

	FWK_ORDER_BY_DECL(MeshBuffers, positions, normals, tex_coords, colors, weights, node_names);

	vector<float3> positions;
	vector<float3> normals;
	vector<float2> tex_coords;
	vector<IColor> colors;
	vector<vector<VertexWeight>> weights;
	vector<string> node_names;
};
}
