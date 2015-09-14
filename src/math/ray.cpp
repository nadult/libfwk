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

	return lmin <= lmax ? make_pair(lmin, lmax) : make_pair(constant::inf, constant::inf);
}

pair<float, float> intersectionRange(const Segment &segment, const Box<float3> &box) {
	auto pair = intersectionRange((Ray)segment, box);
	pair.second = min(pair.second, segment.length());
	return pair.first <= pair.second ? pair : make_pair(constant::inf, constant::inf);
}

// Moller-Trombore, source: wikipedia
float intersection(const Ray &ray, const Triangle &tri) {
	float3 e1 = tri.edge1(), e2 = tri.edge2();

	// Begin calculating determinant - also used to calculate u parameter
	float3 P = cross(normalize(ray.dir()), e2);
	// if determinant is near zero, ray lies in plane of triangle
	float det = dot(e1, P);
	// NOT CULLING
	if(det > -constant::epsilon && det < constant::epsilon)
		return constant::inf;
	float inv_det = 1.f / det;

	// calculate distance from V1 to ray origin
	float3 T = ray.origin() - tri.a();

	// Calculate u parameter and test bound
	float u = dot(T, P) * inv_det;
	// The intersection lies outside of the triangle
	if(u < 0.f || u > 1.f)
		return constant::inf;

	// Prepare to test v parameter
	float3 Q = cross(T, e1);

	// Calculate V parameter and test bound
	float v = dot(normalize(ray.dir()), Q) * inv_det;
	// The intersection lies outside of the triangle
	if(v < 0.f || u + v > 1.f)
		return constant::inf;

	float t = dot(e2, Q) * inv_det;

	if(t > constant::epsilon)
		return t;

	return constant::inf;
}

float intersection(const Segment &segment, const Triangle &tri) {
	float isect = intersection((Ray)segment, tri);
	return isect < 0.0f || isect > segment.length() ? constant::inf : isect;
}

float intersection(const Ray &ray, const Plane &plane) {
	float ndot = dot(plane.normal(), ray.dir());
	if(fabs(ndot) < constant::epsilon)
		return constant::inf;
	return -dot(plane, ray.origin()) / ndot;
}

float intersection(const Segment &segment, const Plane &plane) {
	float dist = intersection((Ray)segment, plane);
	return dist < 0.0f || dist > segment.length() ? constant::inf : dist;
}

const Segment operator*(const Matrix4 &mat, const Segment &segment) {
	return Segment(mulPoint(mat, segment.origin()), mulPoint(mat, segment.end()));
}
}
