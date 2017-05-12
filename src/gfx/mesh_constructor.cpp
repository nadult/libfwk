/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

This file is part of libfwk.*/

#include "fwk_mesh.h"
#include <algorithm>

namespace fwk {

Mesh Mesh::makeRect(const FRect &xz_rect, float y) {
	auto positions = {
		float3(xz_rect.min[0], y, xz_rect.min[1]), float3(xz_rect.max[0], y, xz_rect.min[1]),
		float3(xz_rect.max[0], y, xz_rect.max[1]), float3(xz_rect.min[0], y, xz_rect.max[1])};
	auto normals = vector<float3>(4, float3(0, 1, 0));
	auto tex_coords = vector<float2>{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	return Mesh({move(positions), move(normals), move(tex_coords)},
				{MeshIndices({0, 2, 1, 0, 3, 2})});
}

Mesh Mesh::makeBBox(const FBox &bbox) {
	auto corners = bbox.corners();
	float2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	int order[] = {1, 3, 2, 0, 1, 0, 4, 5, 5, 4, 6, 7, 3, 1, 5, 7, 2, 6, 4, 0, 3, 7, 6, 2};

	vector<float3> positions;
	vector<float2> tex_coords;
	positions.reserve(24);
	tex_coords.reserve(24);

	vector<uint> indices;
	indices.reserve(36);

	for(int s = 0; s < 6; s++) {
		for(int i = 0; i < 4; i++) {
			positions.emplace_back(corners[order[s * 4 + i]]);
			tex_coords.emplace_back(uvs[i]);
		}

		int face_indices[6] = {2, 1, 0, 3, 2, 0};
		for(int i = 0; i < 6; i++)
			indices.push_back(s * 4 + face_indices[i]);
	}

	return Mesh({positions, {}, tex_coords}, {{indices}});
}

Mesh Mesh::makeCylinder(const Cylinder &cylinder, int num_sides) {
	DASSERT(num_sides >= 3);

	vector<float3> positions(num_sides * 2);
	for(int n = 0; n < num_sides; n++) {
		float angle = n * fconstant::pi * 2.0f / float(num_sides);
		float px = cosf(angle) * cylinder.radius();
		float pz = sinf(angle) * cylinder.radius();
		float3 offset = cylinder.pos();

		positions[n] = float3(px, 0.0f, pz) + offset;
		positions[n + num_sides] = float3(px, cylinder.height(), pz) + offset;
	}

	// TODO: generate tex coords as well?
	// TODO: create mapping functions (like in 3dsmax) and apply them afterwards

	vector<uint> indices;
	indices.reserve(num_sides * 6 + (num_sides - 2) * 3 * 2);
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

	return Mesh({positions, {}, {}}, {{indices}});
}

Mesh Mesh::makeTetrahedron(const Tetrahedron &tet) {
	vector<float3> positions;
	insertBack(positions, tet.verts());

	vector<uint> indices;
	for(int n = 0; n < 4; n++) {
		int inds[4] = {n, (n + 1) % 4, (n + 2) % 4, (n + 3) % 4};

		if(dot(Plane(tet[inds[0]], tet[inds[1]], tet[inds[2]]), tet[inds[3]]) > 0.0f)
			swap(inds[1], inds[2]);
		insertBack(indices, {inds[0], inds[1], inds[2]});
	}

	return Mesh({positions, {}, {}}, {{indices}});
}

Mesh Mesh::makePlane(const Plane &plane, const float3 &start, float size) {
	DASSERT(size > fconstant::epsilon);
	FATAL("Test me");

	float3 p[3] = {{-size, -size, -size}, {size, size, size}, {size, -size, size}};
	for(int i = 0; i < 3; i++)
		p[i] = closestPoint(plane, p[i]);
	if(distance(p[0], p[1]) < distance(p[0], p[2]))
		p[1] = p[2];

	float3 pnormal1 = normalize(p[1] - p[0]);
	float3 pnormal2 = cross(plane.normal(), pnormal1);

	float3 center = closestPoint(plane, start);
	float3 corners[4] = {
		center - pnormal1 * size - pnormal2 * size, center + pnormal1 * size - pnormal2 * size,
		center + pnormal1 * size + pnormal2 * size, center - pnormal1 * size + pnormal2 * size,
	};

	vector<uint> indices = {0, 1, 2, 0, 2, 3};
	return Mesh({vector<float3>(corners, corners + 4), {}, {}}, {{indices}});
}

Mesh Mesh::makePolySoup(CRange<Triangle> rtris) {
	vector<float3> positions;
	vector<uint> indices;

	vector<Triangle> tris = rtris;
	std::sort(begin(tris), end(tris), [](const auto &a, const auto &b) {
		auto ca = a.center(), cb = b.center();
		return std::tie(ca.x, ca.y, ca.z) < std::tie(cb.x, cb.y, cb.z);
	});

	for(const auto &tri : tris) {
		uint off = positions.size();
		int inds[3];
		for(auto vert : tri.verts()) {
			auto result = findMin(positions, [vert](auto v) { return distanceSq(v, vert); });

			int index = 0;
			if(result.first != -1 && sqrtf(result.second) < fconstant::epsilon)
				index = result.first;
			else {
				index = positions.size();
				positions.emplace_back(vert);
			}
			indices.emplace_back((uint)index);
		}
	}

	return Mesh({positions, {}, {}}, {{indices}});
}
}
