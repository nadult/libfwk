/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

#ifdef FWK_TARGET_LINUX
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/intersections.h>
#include <CGAL/Triangle_3.h>
#include <boost/variant.hpp>
#endif
#include "fwk_profile.h"

namespace fwk {

#ifdef FWK_TARGET_LINUX
bool areIntersecting(const Triangle &a, const Triangle &b) {
	using K = CGAL::Exact_predicates_exact_constructions_kernel;
	using Point = K::Point_3;
	using Segment = K::Segment_3;

	Point ap[3], bp[3];
	for(int n = 0; n < 3; n++)
		ap[n] = Point(a[n].x, a[n].y, a[n].z);
	for(int n = 0; n < 3; n++)
		bp[n] = Point(b[n].x, b[n].y, b[n].z);

	CGAL::Triangle_3<K> tria(ap[0], ap[1], ap[2]);
	CGAL::Triangle_3<K> trib(bp[0], bp[1], bp[2]);

	try {
		auto isect = intersection(tria, trib);
		if(isect) {
			if(auto *p = boost::get<Point>(&*isect)) {
				// printf("single point");
				return false;
			}
			if(auto *s = boost::get<Segment>(&*isect)) {
				float len = sqrtf(CGAL::to_double(s->squared_length()));
				// printf("segment: %f\n", len);
				return len > constant::epsilon;
			}
			if(auto *t = boost::get<K::Triangle_3>(&*isect)) {
				// printf("tri\n");
			}
			if(auto *pp = boost::get<vector<Point>>(&*isect)) {
				// printf("points\n");
			}
			return true;
		}
	} catch(...) { return true; }

	return false;
}

bool areIntersecting(const Triangle2D &a, const Triangle2D &b) {
	using K = CGAL::Exact_predicates_exact_constructions_kernel;
	using Point = K::Point_2;
	using Segment = K::Segment_2;

	Point ap[3], bp[3];
	for(int n = 0; n < 3; n++)
		ap[n] = Point(a[n].x, a[n].y);
	for(int n = 0; n < 3; n++)
		bp[n] = Point(b[n].x, b[n].y);

	CGAL::Triangle_2<K> tria(ap[0], ap[1], ap[2]);
	CGAL::Triangle_2<K> trib(bp[0], bp[1], bp[2]);

	try {
		auto isect = intersection(tria, trib);
		if(isect) {
			if(auto *p = boost::get<Point>(&*isect))
				return false;
			if(auto *s = boost::get<Segment>(&*isect)) {
				float len = sqrtf(CGAL::to_double(s->squared_length()));
				return len > constant::epsilon;
			}

			return true;
		}
	} catch(...) { return true; }

	return false;
}
#endif

Triangle::Triangle(const float3 &a, const float3 &b, const float3 &c) {
	m_point = a;
	m_edge[0] = b - a;
	m_edge[1] = c - a;
	m_cross = fwk::cross(edge1(), edge2());
}

// TODO: write tests
float3 Triangle::normal() const { return normalize(m_cross); }
float Triangle::area() const { return 0.5f * length(m_cross); }

Triangle operator*(const Matrix4 &mat, const Triangle &tri) {
	return Triangle(mulPoint(mat, tri.a()), mulPoint(mat, tri.b()), mulPoint(mat, tri.c()));
}

float distance(const Triangle &a, const Triangle &b) {
	float min_dist = constant::inf;
	for(int n = 0; n < 3 && min_dist > constant::epsilon; n++)
		min_dist = min(min_dist, distance(b, Segment(a[n], a[(n + 1) % 3])));
	for(int n = 0; n < 3 && min_dist > constant::epsilon; n++)
		min_dist = min(min_dist, distance(a, Segment(b[n], b[(n + 1) % 3])));
	return min_dist;
}

float distance(const Triangle &tri, const Segment &seg) {
	float isect = intersection(tri, seg);
	if(isect < constant::inf)
		return 0.0f;

	float plane_isect = intersection(Plane(tri), (Ray)seg);
	float3 point = plane_isect < 0.0f ? seg.origin() : plane_isect > seg.length()
														   ? seg.end()
														   : seg.at(plane_isect);
	return distance(tri, point);
}

// Source: realtimecollisiondetection (book)
float3 closestPoint(const Triangle &tri, const float3 &point) {
	float3 a = tri.a(), b = tri.b(), c = tri.c();

	// Check if P in vertex region outside A
	float3 ab = tri.b() - tri.a();
	float3 ac = tri.c() - tri.a();
	float3 ap = point - tri.a();
	float d1 = dot(ab, ap);
	float d2 = dot(ac, ap);
	if(d1 <= 0.0f && d2 <= 0.0f)
		return a; // barycentric coordinates (1,0,0)

	// Check if P in vertex region outside B
	float3 bp = point - b;
	float d3 = dot(ab, bp);
	float d4 = dot(ac, bp);
	if(d3 >= 0.0f && d4 <= d3)
		return b; // barycentric coordinates (0,1,0)

	// Check if P in edge region of AB, if so return projection of P onto AB
	float vc = d1 * d4 - d3 * d2;
	if(vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		float v = d1 / (d1 - d3);
		return a + ab * v; // barycentric coordinates (1-v,v,0)
	}

	// Check if P in vertex region outside C
	float3 cp = point - c;
	float d5 = dot(ab, cp);
	float d6 = dot(ac, cp);
	if(d6 >= 0.0f && d5 <= d6)
		return c; // barycentric coordinates (0,0,1)

	// Check if P in edge region of AC, if so return projection of P onto AC
	float vb = d5 * d2 - d1 * d6;
	if(vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		float w = d2 / (d2 - d6);
		return a + ac * w; // barycentric coordinates (1-w,0,w)
	}

	// Check if P in edge region of BC, if so return projection of P onto BC
	float va = d3 * d6 - d5 * d4;
	if(va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return b + (c - b) * w; // barycentric coordinates (0,1-w,w)
	}

	// P inside face region. Compute Q through its barycentric coordinates (u,v,w)
	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;
	return a + ab * v + ac * w; // = u*a + v*b + w*c, u = va * denom = 1.0f - v - w
}

float distance(const Triangle &tri, const float3 &point) {
	return distance(closestPoint(tri, point), point);
}
}
