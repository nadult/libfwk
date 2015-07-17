/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <deque>
#include <unordered_map>

namespace fwk {

SimpleMesh::SimpleMesh(MakeRect, const FRect &xz_rect, float y)
	: SimpleMesh(MeshBuffers({float3(xz_rect.min[0], y, xz_rect.min[1]),
							  float3(xz_rect.max[0], y, xz_rect.min[1]),
							  float3(xz_rect.max[0], y, xz_rect.max[1]),
							  float3(xz_rect.min[0], y, xz_rect.max[1])},
							 vector<float3>(4, float3(0, 1, 0)), {{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
				 {MeshIndices({0, 2, 1, 0, 3, 2})}) {}

SimpleMesh::SimpleMesh(MakeBBox, const FBox &bbox) : SimpleMesh() {
	float3 corners[8];
	float2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	bbox.getCorners(corners);

	int order[] = {1, 3, 2, 0, 1, 0, 4, 5, 5, 4, 6, 7, 3, 1, 5, 7, 2, 6, 4, 0, 3, 7, 6, 2};

	m_buffers.positions.reserve(24);
	m_buffers.tex_coords.reserve(24);

	vector<uint> indices;
	indices.reserve(36);

	for(int s = 0; s < 6; s++) {
		for(int i = 0; i < 4; i++) {
			m_buffers.positions.emplace_back(corners[order[s * 4 + i]]);
			m_buffers.tex_coords.emplace_back(uvs[i]);
		}

		int face_indices[6] = {2, 1, 0, 3, 2, 0};
		for(int i = 0; i < 6; i++)
			indices.push_back(s * 4 + face_indices[i]);
	}

	m_indices = {MeshIndices(indices)};
	afterInit();
}

SimpleMesh::SimpleMesh(MakeCylinder, const Cylinder &cylinder, int num_sides) : SimpleMesh() {
	DASSERT(num_sides >= 3);

	m_buffers.positions.resize(num_sides * 2);
	for(int n = 0; n < num_sides; n++) {
		float angle = n * constant::pi * 2.0f / float(num_sides);
		float px = cosf(angle) * cylinder.radius();
		float pz = sinf(angle) * cylinder.radius();
		float3 offset = cylinder.pos();

		m_buffers.positions[n] = float3(px, 0.0f, pz) + offset;
		m_buffers.positions[n + num_sides] = float3(px, cylinder.height(), pz) + offset;
	}

	// TODO: generate tex coords as well?
	// TODO: create mapping functions (like in 3dsmax) and apply them afterwards

	vector<uint> indices;
	m_indices.reserve(num_sides * 6 + (num_sides - 2) * 3 * 2);
	for(int n = 0; n < num_sides; n++) {
		uint i0 = n, i1 = (n + 1) % num_sides;
		uint j0 = n + num_sides, j1 = i1 + num_sides;

		indices.insert(end(indices), {i0, j1, i1, i0, j0, j1});
	}

	for(int t = 0; t < num_sides - 2; t++) {
		uint i0 = 0, i1 = t + 1, i2 = t + 2;
		indices.insert(end(indices), {i0, i1, i2});
	}
	for(int t = 0; t < num_sides - 2; t++) {
		uint i0 = 0 + num_sides, i1 = t + 1 + num_sides, i2 = t + 2 + num_sides;
		indices.insert(end(indices), {i0, i2, i1});
	}

	m_indices = {MeshIndices(indices)};
	afterInit();
}
}
