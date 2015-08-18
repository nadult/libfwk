/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include <cmath>
#include <cstdlib>

namespace fwk {

Ray::Ray(const float3 &origin, const float3 &dir)
	: m_origin(origin), m_dir(dir), m_inv_dir(inv(dir)) {}

Ray::Ray(const Matrix4 &screen_to_world, const float2 &screen_pos) {
	float3 p1 = mulPoint(screen_to_world, float3(screen_pos.x, screen_pos.y, 0.0f));
	float3 p2 = mulPoint(screen_to_world, float3(screen_pos.x, screen_pos.y, 100.0f));

	m_origin = p1;
	m_dir = normalize(p2 - p1);
	m_inv_dir = float3(1.0f / m_dir.x, 1.0f / m_dir.y, 1.0f / m_dir.z);
}

Segment::Segment(const Ray &ray, float min, float max) : Ray(ray), m_min(min), m_max(max) {
}
Segment::Segment(const float3 &start, const float3 &end)
	: Ray(start, normalize(end - start)), m_min(0.0f), m_max(distance(end, start)) {}

Segment Segment::operator-() const { THROW("wtf");return Segment(Ray::operator-(), m_min, m_max); }

float distance(const Ray &ray, const float3 &point) {
	float3 diff = point - ray.origin();
	float t = dot(diff, ray.dir());

	if(t > 0.0f)
		diff -= ray.dir() * t;
	return dot(diff, diff);
}

pair<float, float> intersectionRange(const Segment &segment, const Box<float3> &box) {
	// TODO: check if works correctly for (+/-)INF
	float3 inv_dir = segment.invDir();
	float3 origin = segment.origin();

	float l1 = inv_dir.x * (box.min.x - origin.x);
	float l2 = inv_dir.x * (box.max.x - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1 = inv_dir.y * (box.min.y - origin.y);
	l2 = inv_dir.y * (box.max.y - origin.y);
	lmin = max(lmin, min(l1, l2));
	lmax = min(lmax, max(l1, l2));

	l1 = inv_dir.z * (box.min.z - origin.z);
	l2 = inv_dir.z * (box.max.z - origin.z);
	lmin = max(lmin, min(l1, l2));
	lmax = min(lmax, max(l1, l2));
	lmin = max(lmin, segment.min());
	lmax = min(lmax, segment.max());

	return lmin <= lmax ? make_pair(lmin, lmax) : make_pair(constant::inf, constant::inf);
}

float intersection(const Segment &segment, const Box<float3> &box) {
	auto ranges = intersectionRange(segment, box);
	return ranges.first;
}

float intersection(const Segment &segment, const Triangle &tri) {
	float tri_dot = dot(segment.dir(), tri.cross());

	float3 offset = segment.origin() - tri.a();
	float3 e1 = tri.edge1(), e2 = tri.edge2();
	if(tri_dot < 0) { // back side
		swap(e1, e2);
		tri_dot = -tri_dot;
	}

	float3 tmp = cross(segment.dir(), offset);
	float v = dot(e2, tmp);
	float w = -dot(e1, tmp);

	if(v < 0.0f || v > tri_dot || w < 0.0f || v + w > tri_dot)
		return constant::inf;

	return intersection(Plane(tri), segment);
}

float intersection(const Segment &segment, const Plane &plane) {
	//TOODO: should be working for both sides
	float dist = -dot(plane, segment.origin()) / dot(plane.normal(), segment.dir());
	return dist < segment.min() || dist > segment.max() ? constant::inf : dist;
}

const Segment operator*(const Matrix4 &mat, const Segment &segment) {
	if(segment.min() > -constant::inf && segment.max() < constant::inf) {
		float3 start = segment.at(segment.min()), end = segment.at(segment.max());
		return Segment(mulPoint(mat, start), mulPoint(mat, end));
	}

	float3 new_origin = mulPoint(mat, segment.origin());
	float3 new_dir = mulNormal(mat, segment.dir());
	float len = 1.0f;

	return Segment(Ray(new_origin, new_dir), segment.min() * len, segment.max() * len);
}
}
