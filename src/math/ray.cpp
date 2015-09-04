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
	m_inv_dir = inverse(m_dir);
}

Segment::Segment(const float3 &start, const float3 &end)
	: Ray(start, normalize(end - start)), m_end(end), m_length(distance(end, start)) {
	DASSERT(m_length > 0.0f);
}

float distance(const Ray &ray, const float3 &point) {
	float3 diff = point - ray.origin();
	float t = dot(diff, ray.dir());

	if(t > 0.0f)
		diff -= ray.dir() * t;
	return dot(diff, diff);
}

pair<float, float> intersectionRange(const Ray &ray, const Box<float3> &box) {
	float3 inv_dir = ray.invDir();
	float3 origin = ray.origin();

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
	lmin = max(lmin, 0.0f);

	return lmin <= lmax ? make_pair(lmin, lmax) : make_pair(constant::inf, constant::inf);
}

pair<float, float> intersectionRange(const Segment &segment, const Box<float3> &box) {
	auto pair = intersectionRange((Ray)segment, box);
	pair.second = min(pair.second, segment.length());
	return pair.first <= pair.second ? pair : make_pair(constant::inf, constant::inf);
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
	// TODO: should be working for both sides
	float dist = -dot(plane, segment.origin()) / dot(plane.normal(), segment.dir());
	return dist < 0.0f || dist > segment.length() ? constant::inf : dist;
}

const Segment operator*(const Matrix4 &mat, const Segment &segment) {
	return Segment(mulPoint(mat, segment.origin()), mulPoint(mat, segment.end()));
}
}
