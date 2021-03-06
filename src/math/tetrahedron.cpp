// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/tetrahedron.h"

#include "fwk/math/sat_test.h"
#include "fwk/math/triangle.h"

namespace fwk {
Tetrahedron::Tetrahedron(const float3 &p1, const float3 &p2, const float3 &p3, const float3 &p4)
	: m_verts({{p1, p2, p3, p4}}) {}

array<array<int, 3>, 4> Tetrahedron::faces() {
	array<FaceIndices, 4> out;
	out[0] = {{0, 1, 2}};
	out[1] = {{1, 3, 2}};
	out[2] = {{2, 3, 0}};
	out[3] = {{3, 1, 0}};
	return out;
}

array<Plane3F, 4> Tetrahedron::planes() const {
	auto faces = Tetrahedron::faces();
	array<Plane3F, 4> out;
	for(int n = 0; n < 4; n++)
		out[n] = Plane3F(m_verts[faces[n][0]], m_verts[faces[n][1]], m_verts[faces[n][2]]);
	return out;
}

array<Triangle3F, 4> Tetrahedron::tris() const {
	auto faces = Tetrahedron::faces();
	array<Triangle3F, 4> out;
	for(int n = 0; n < 4; n++)
		out[n] = Triangle3F(m_verts[faces[n][0]], m_verts[faces[n][1]], m_verts[faces[n][2]]);
	return out;
}

array<Tetrahedron::Edge, 6> Tetrahedron::edges() const {
	int indices[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {3, 1}, {3, 2}};
	array<Edge, 6> out;
	for(int n = 0; n < 6; n++)
		out[n] = Edge{m_verts[indices[n][0]], m_verts[indices[n][1]]};
	return out;
}

float Tetrahedron::volume() const {
	return dot(m_verts[0] - m_verts[3], cross(m_verts[1] - m_verts[3], m_verts[2] - m_verts[3])) /
		   6.0f;
}

float Tetrahedron::surfaceArea() const {
	float total = 0.0f;
	for(auto tri : tris())
		total += tri.surfaceArea();
	return total;
}

float Tetrahedron::inscribedSphereRadius() const { return 3.0f * volume() / surfaceArea(); }

bool Tetrahedron::isValid() const {
	for(int n = 0; n < 4; n++)
		if(distance(m_verts[n], m_verts[(n + 1) % 4]) < epsilon<float>)
			return false;
	return volume() > std::pow(epsilon<float>, 3);
}

bool Tetrahedron::isInside(const float3 &point) const {
	for(const auto &plane : planes())
		if(plane.signedDistance(point) > 0.0f)
			return false;
	return true;
}

Tetrahedron fixVolume(const Tetrahedron &tet) {
	if(tet.volume() < 0.0f)
		return {tet[3], tet[1], tet[2], tet[0]};
	return tet;
}

bool areIntersecting(const Tetrahedron &a, const Tetrahedron &b) { return satTest(a, b); }
}
