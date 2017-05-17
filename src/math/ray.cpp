/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_xml.h"
#include <cmath>
#include <cstdlib>

namespace fwk {

/*
Ray::Ray(const Matrix4 &screen_to_world, const float2 &screen_pos) {
	float3 p1 = mulPoint(screen_to_world, float3(screen_pos.x, screen_pos.y, 0.0f));
	float3 p2 = mulPoint(screen_to_world, float3(screen_pos.x, screen_pos.y, 100.0f));

	m_origin = p1;
	m_dir = normalize(p2 - p1);
	m_inv_dir = inv(m_dir);
}*/

float3 closestPoint(const Ray &ray, const float3 &point) {
	float3 diff = point - ray.origin();
	float t = dot(diff, ray.dir());
	return ray.origin() + ray.dir() * t;
}

// Algorithm idea: http://www.geometrictools.com/GTEngine/Include/Mathematics/GteDistLineLine.h
pair<float3, float3> closestPoints(const Ray &ray1, const Ray &ray2) {
	float3 diff = ray1.origin() - ray2.origin();
	float a01 = -dot(ray1.dir(), ray2.dir());
	float b0 = dot(diff, ray1.dir());
	float s1 = -b0, s2 = 0.0f;

	if(fabs(a01) < 1.0f) {
		float det = 1.0f - a01 * a01;
		float b1 = -dot(diff, ray2.dir());
		s1 = (a01 * b1 - b0) / det;
		s2 = (a01 * b0 - b1) / det;
	}

	return make_pair(ray1.at(s1), ray2.at(s2));
}

float distance(const Ray &ray, const float3 &point) {
	return distance(closestPoint(ray, point), point);
}

float distance(const Ray &seg1, const Ray &seg2) {
	auto points = closestPoints(seg1, seg2);
	return distance(points.first, points.second);
}

pair<float, float> intersectionRange(const Ray &ray, const Box<float3> &box) {
	float3 inv_dir = ray.invDir();
	float3 origin = ray.origin();

	float l1 = inv_dir.x * (box.x() - origin.x);
	float l2 = inv_dir.x * (box.ex() - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1 = inv_dir.y * (box.y() - origin.y);
	l2 = inv_dir.y * (box.ey() - origin.y);
	lmin = max(lmin, min(l1, l2));
	lmax = min(lmax, max(l1, l2));

	l1 = inv_dir.z * (box.z() - origin.z);
	l2 = inv_dir.z * (box.ez() - origin.z);
	lmin = max(lmin, min(l1, l2));
	lmax = min(lmax, max(l1, l2));

	return lmin <= lmax ? make_pair(lmin, lmax) : make_pair(fconstant::inf, fconstant::inf);
}

// Moller-Trombore, source: wikipedia
float intersection(const Ray &ray, const Triangle &tri) {
	float3 e1 = tri.edge1(), e2 = tri.edge2();

	// Begin calculating determinant - also used to calculate u parameter
	float3 P = cross(normalize(ray.dir()), e2);
	float det = dot(e1, P);

	// if determinant is near zero, ray lies in plane of triangle
	if(det > -fconstant::isect_epsilon && det < fconstant::isect_epsilon)
		return fconstant::inf;
	float inv_det = 1.f / det;

	// calculate distance from V1 to ray origin
	float3 T = ray.origin() - tri.a();

	// Calculate u parameter and test bound
	float u = dot(T, P) * inv_det;
	// The intersection lies outside of the triangle
	if(u < 0.f || u > 1.f)
		return fconstant::inf;

	// Prepare to test v parameter
	float3 Q = cross(T, e1);

	// Calculate V parameter and test bound
	float v = dot(normalize(ray.dir()), Q) * inv_det;
	// The intersection lies outside of the triangle
	if(v < 0.f || u + v > 1.f)
		return fconstant::inf;

	float t = dot(e2, Q) * inv_det;

	if(t > fconstant::epsilon)
		return t;

	return fconstant::inf;
}

float intersection(const Ray &ray, const Plane &plane) {
	float ndot = dot(plane.normal(), ray.dir());
	if(fabs(ndot) < fconstant::epsilon)
		return fconstant::inf;
	return -dot(plane, ray.origin()) / ndot;
}
}
