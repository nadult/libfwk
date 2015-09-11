/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include <CGAL/Simple_cartesian.h>
#include <CGAL/intersections.h>
#include <CGAL/Tetrahedron_3.h>
#include <CGAL/Triangle_3.h>

namespace fwk {
Tetrahedron::Tetrahedron(const float3 &p1, const float3 &p2, const float3 &p3, const float3 &p4)
	: m_corners({{p1, p2, p3, p4}}) {
	if(volume() < 0.0f)
		swap(m_corners[2], m_corners[3]);
}

bool Tetrahedron::isIntersecting(const Triangle &triangle) const {
	using K = CGAL::Simple_cartesian<double>;
	using Point = K::Point_3;

	float3 center = (m_corners[0] + m_corners[1] + m_corners[2] + m_corners[3]) * 0.25f;

	Point tet_points[4], tri_points[3];
	for(int n = 0; n < 4; n++) {
		float3 p = lerp(m_corners[n], center, 0.001f);
		tet_points[n] = Point(p.x, p.y, p.z);
	}
	for(int n = 0; n < 3; n++)
		tri_points[n] = Point(triangle[n].x, triangle[n].y, triangle[n].z);

	CGAL::Tetrahedron_3<K> tet(tet_points[0], tet_points[1], tet_points[2], tet_points[3]);
	CGAL::Triangle_3<K> tri(tri_points[0], tri_points[1], tri_points[2]);

	try {
		return CGAL::do_intersect(tet, tri);
	} catch(...) {}

	return true;
}

float Tetrahedron::volume() const {
	return dot(m_corners[0] - m_corners[3],
			   cross(m_corners[1] - m_corners[3], m_corners[2] - m_corners[3])) /
		   6.0f;
}

bool Tetrahedron::isValid() const {
	for(int n = 0; n < 4; n++)
		if(distance(m_corners[n], m_corners[(n + 1) % 4]) < constant::epsilon)
			return false;
	return volume() > pow(constant::epsilon, 3);
}
}
