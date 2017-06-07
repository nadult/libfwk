/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_xml.h"

#ifdef CGAL_ENABLED
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/intersections.h>
#include <CGAL/Triangle_3.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Polygon_2.h>
#include <boost/variant.hpp>
#endif
#include "fwk_profile.h"

namespace fwk {

#ifdef CGAL_ENABLED
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
				return len > fconstant::epsilon;
			}
			if(auto *t = boost::get<K::Triangle_3>(&*isect)) {
				// printf("tri\n");
			}
			if(auto *pp = boost::get<vector<Point>>(&*isect)) {
				// printf("points\n");
			}
			return true;
		}
	} catch(...) {
		return true;
	}

	return false;
}

template <class K> float3 fromCGAL(const CGAL::Point_3<K> &p) {
	return float3(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
}

template <class K> float2 fromCGAL(const CGAL::Point_2<K> &p) {
	return float2(CGAL::to_double(p.x()), CGAL::to_double(p.y()));
}

pair<Segment3<float>, bool> intersectionSegment(const Triangle &a, const Triangle &b) {
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
			if(auto *s = boost::get<Segment>(&*isect)) {
				float len = sqrtf(CGAL::to_double(s->squared_length()));
				if(len > fconstant::epsilon)
					return make_pair(fwk::Segment3<float>(fromCGAL(s->min()), fromCGAL(s->max())),
									 true);
			}
		}
	} catch(...) {
	}

	return make_pair(fwk::Segment3<float>(), false);
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
				return len > fconstant::epsilon;
			}

			return true;
		}
	} catch(...) {
		return true;
	}

	return false;
}

#endif

Triangle::Triangle(const float3 &a, const float3 &b, const float3 &c) {
	m_point = a;
	m_edge[0] = b - a;
	m_edge[1] = c - a;
	m_normal = fwk::cross(edge1(), edge2());
	m_length = length(m_normal);
	m_normal /= m_length;
}

Triangle operator*(const Matrix4 &mat, const Triangle &tri) {
	return Triangle(mulPoint(mat, tri.a()), mulPoint(mat, tri.b()), mulPoint(mat, tri.c()));
}

float distance(const Triangle &a, const Triangle &b) {
	float min_dist = fconstant::inf;
	for(int n = 0; n < 3 && min_dist > fconstant::epsilon; n++)
		min_dist = min(min_dist, distance(b, Segment3<float>(a[n], a[(n + 1) % 3])));
	for(int n = 0; n < 3 && min_dist > fconstant::epsilon; n++)
		min_dist = min(min_dist, distance(a, Segment3<float>(b[n], b[(n + 1) % 3])));
	return min_dist;
}

float distance(const Triangle &tri, const Segment3<float> &seg) {
	if(seg.empty())
		return distance(tri, seg.from);

	float isect = intersection(tri, seg);
	if(isect < fconstant::inf)
		return 0.0f;

	float plane_isect = intersection(Plane(tri), *seg.asRay()) / seg.length();
	float3 point =
		plane_isect < 0.0f ? seg.from : plane_isect > 1.0f ? seg.to : seg.at(plane_isect);
	return distance(tri, point);
}

float3 Triangle::barycentric(const float3 &point) const {
	float3 diff = point - m_point;
	float w = dot(fwk::cross(m_edge[0], diff), m_normal);
	float v = dot(fwk::cross(m_edge[1], diff), m_normal);
	float u = 1.0f - w - v;
	return float3(u, v, w);
}

vector<float3> Triangle::pickPoints(float density) const {
	float stepx = 1.0f / (length(m_edge[0]) * density);
	float stepy = 1.0f / (length(m_edge[1]) * density);
	vector<float3> out;

	for(float x = 0; x < 1.0f; x += stepx)
		for(float y = 0; x + y < 1.0f; y += stepy)
			out.emplace_back(m_edge[0] * x + m_edge[1] * y + m_point);

	return out;
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

float distance(const Triangle2D &tri, const float2 &point) {
	return distance(Triangle(asXZ(tri[0]), asXZ(tri[1]), asXZ(tri[2])),
					float3(point.x, 0.0f, point.y));
}

array<float, 3> Triangle::angles() const {
	array<float, 3> out;
	Projection proj(*this);

	auto verts = this->verts();
	float2 pverts[3];
	for(int i = 0; i < 3; i++)
		pverts[i] = (proj * verts[i]).xz();
	float2 dirs[3];
	for(int i = 0; i < 3; i++)
		dirs[i] = normalize(pverts[i] - pverts[(i + 2) % 3]);
	for(int n = 0; n < 3; n++)
		out[n] = fconstant::pi * 2.0f - angleBetween(-dirs[n], dirs[(n + 1) % 3]);

	return out;
}
}
