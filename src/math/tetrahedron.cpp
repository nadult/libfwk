/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include <CGAL/Simple_cartesian.h>
#include <CGAL/intersections.h>
#include <CGAL/Tetrahedron_3.h>
#include <CGAL/Triangle_3.h>

namespace fwk {
Tetrahedron::Tetrahedron(const float3 &p1, const float3 &p2, const float3 &p3, const float3 &p4)
	: m_corners({{p1, p2, p3, p4}}) {}

bool Tetrahedron::isIntersecting(const Triangle &triangle0) const {
	using K = CGAL::Simple_cartesian<double>;
	using Point = K::Point_3;

	Triangle triangle(lerp(triangle0[0], triangle0.center(), 0.01f),
					  lerp(triangle0[1], triangle0.center(), 0.01f),
					  lerp(triangle0[2], triangle0.center(), 0.01f));

	Point tet_points[4], tri_points[3];
	for(int n = 0; n < 4; n++)
		tet_points[n] = Point(m_corners[n].x, m_corners[n].y, m_corners[n].z);
	for(int n = 0; n < 3; n++)
		tri_points[n] = Point(triangle[n].x, triangle[n].y, triangle[n].z);

	CGAL::Tetrahedron_3<K> tet(tet_points[0], tet_points[1], tet_points[2], tet_points[3]);
	CGAL::Triangle_3<K> tri(tri_points[0], tri_points[1], tri_points[2]);

	try {
		return CGAL::do_intersect(tet, tri);
	} catch(...) {}

	return true;
}
}
