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

float distance(const Ray &ray, const float3 &point) {
	float3 diff = point - ray.origin();
	float t = dot(diff, ray.dir());

	if(t > 0.0f)
		diff -= ray.dir() * t;
	return dot(diff, diff);
}

float intersection(const Segment &segment, const Box<float3> &box) {
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
	lmin = max(lmin, segment.min);
	lmax = min(lmax, segment.max);

	return lmin <= lmax ? lmin : constant::inf;
}

float intersection(const Segment &segment, const float3 &a, const float3 &b, const float3 &c) {
	float3 ab = b - a;
	float3 ac = c - a;

	float3 tri_nrm = cross(ab, ac);
	float tri_dot = dot(segment.dir(), tri_nrm);
	if(tri_dot <= 0.0f)
		return constant::inf;

	float3 offset = segment.origin() - a;

	float3 tmp = cross(segment.dir(), offset);
	float v = dot(ac, tmp);
	float w = -dot(ab, tmp);

	if(v < 0.0f || v > tri_dot || w < 0.0f || v + w > tri_dot)
		return constant::inf;

	float t = dot(offset, tri_nrm);
	return t < segment.min || t > segment.max ? constant::inf : t / tri_dot;
}

const Ray operator*(const Matrix4 &mat, const Ray &ray) {
	float3 target = ray.origin() + ray.dir() * 1000.0f;
	float3 new_origin = mulPoint(mat, ray.origin());
	float3 new_dir = normalize(mulPoint(mat, target) - new_origin);
	return Ray(new_origin, new_dir);
}
}
