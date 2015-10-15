/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

#ifdef CGAL_ENABLED
#include <CGAL/Simple_cartesian.h>
#include <CGAL/intersections.h>
#include <CGAL/Tetrahedron_3.h>
#include <CGAL/Triangle_3.h>
#endif

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

array<Plane, 4> Tetrahedron::planes() const {
	auto faces = Tetrahedron::faces();
	array<Plane, 4> out;
	for(int n = 0; n < 4; n++)
		out[n] = Plane(m_verts[faces[n][0]], m_verts[faces[n][1]], m_verts[faces[n][2]]);
	return out;
}

array<Triangle, 4> Tetrahedron::tris() const {
	auto faces = Tetrahedron::faces();
	array<Triangle, 4> out;
	for(int n = 0; n < 4; n++)
		out[n] = Triangle(m_verts[faces[n][0]], m_verts[faces[n][1]], m_verts[faces[n][2]]);
	return out;
}

array<Tetrahedron::Edge, 6> Tetrahedron::edges() const {
	int indices[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {3, 1}, {3, 2}};
	array<Edge, 6> out;
	for(int n = 0; n < 6; n++)
		out[n] = Edge{m_verts[indices[n][0]], m_verts[indices[n][1]]};
	return out;
}

#ifdef CGAL_ENABLED
bool Tetrahedron::isIntersecting(const Triangle &triangle) const {
	using K = CGAL::Simple_cartesian<double>;
	using Point = K::Point_3;

	float3 center = (m_verts[0] + m_verts[1] + m_verts[2] + m_verts[3]) * 0.25f;

	Point tet_points[4], tri_points[3];
	for(int n = 0; n < 4; n++) {
		float3 p = lerp(m_verts[n], center, 0.001f);
		tet_points[n] = Point(p.x, p.y, p.z);
	}
	for(int n = 0; n < 3; n++)
		tri_points[n] = Point(triangle[n].x, triangle[n].y, triangle[n].z);

	CGAL::Tetrahedron_3<K> tet(tet_points[0], tet_points[1], tet_points[2], tet_points[3]);
	CGAL::Triangle_3<K> tri(tri_points[0], tri_points[1], tri_points[2]);

	try {
		return CGAL::do_intersect(tet, tri);
	} catch(...) {
	}

	return true;
}
#endif

float Tetrahedron::volume() const {
	return dot(m_verts[0] - m_verts[3], cross(m_verts[1] - m_verts[3], m_verts[2] - m_verts[3])) /
		   6.0f;
}

bool Tetrahedron::isValid() const {
	for(int n = 0; n < 4; n++)
		if(distance(m_verts[n], m_verts[(n + 1) % 4]) < constant::epsilon)
			return false;
	return volume() > pow(constant::epsilon, 3);
}

bool Tetrahedron::isInside(const float3 &point) const {
	for(const auto &plane : planes())
		if(dot(plane, point) > 0.0f)
			return false;
	return true;
}

bool areIntersecting(const Tetrahedron &a, const Tetrahedron &b) { return satTest(a, b); }

bool areIntersecting(const Tetrahedron &tet, const FBox &box) {
	return areOverlapping(FBox(verts(tet)), box) && satTest(tet, box);
}
}
