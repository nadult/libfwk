/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_xml.h"
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

float3 closestPoint(const Ray &ray, const float3 &point) {
	float3 diff = point - ray.origin();
	float t = dot(diff, ray.dir());
	return ray.origin() + ray.dir() * t;
}

float3 closestPoint(const Segment &segment, const float3 &point) {
	float3 diff = point - segment.origin();
	float t = dot(diff, segment.dir());

	if(t < 0.0f)
		return segment.origin();
	if(t > segment.length())
		return segment.end();
	t /= segment.length();

	return segment.origin() + (segment.end() - segment.origin()) * t;
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

pair<float3, float3> closestPoints(const Segment &seg1, const Segment &seg2) {
	float3 diff = seg1.origin() - seg2.origin();
	float a01 = -dot(seg1.dir(), seg2.dir());
	float b0 = dot(diff, seg1.dir());
	float s1 = -b0, s2 = 0.0f;

	if(fabs(a01) < 1.0f) {
		float det = 1.0f - a01 * a01;
		float b1 = -dot(diff, seg2.dir());
		s1 = (a01 * b1 - b0) / det;
		s2 = (a01 * b0 - b1) / det;
	}

	s1 = clamp(s1, 0.0f, seg1.length());
	s2 = clamp(s2, 0.0f, seg2.length());

	return make_pair(seg1.at(s1), seg2.at(s2));
}
float distance(const Ray &ray, const float3 &point) {
	return distance(closestPoint(ray, point), point);
}

float distance(const Segment &segment, const float3 &point) {
	return distance(closestPoint(segment, point), point);
}

float distance(const Segment &seg1, const Segment &seg2) {
	auto points = closestPoints(seg1, seg2);
	return distance(points.first, points.second);
}

float distance(const Ray &seg1, const Ray &seg2) {
	auto points = closestPoints(seg1, seg2);
	return distance(points.first, points.second);
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
	float det = dot(e1, P);

	// if determinant is near zero, ray lies in plane of triangle
	if(det > -constant::isect_epsilon && det < constant::isect_epsilon)
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

inline float2 project(const float2 &vx, const float2 &vy, const float2 &vec) {
	return float2(vec.x * vx.x + vec.y * vx.y, vec.x * vy.x + vec.y * vy.y);
}

pair<float2, bool> lineIntersection(const Segment2D &seg1, const Segment2D &seg2) {
	DASSERT(length(seg1) > constant::epsilon);
	DASSERT(length(seg2) > constant::epsilon);

	float2 dir = normalize(seg1.end - seg1.start);
	float2 nrm(-dir.y, dir.x);

	Segment2D proj_seg2(project(dir, nrm, seg2.start - seg1.start),
						project(dir, nrm, seg2.end - seg1.start));

	float2 sdir = proj_seg2.end - proj_seg2.start;

	// TODO: test overlap
	if(fabs(sdir.y) < constant::epsilon) {
		// TODO: fix this
		float2 p1[2] = {seg1.start, seg1.end}, p2[2] = {seg2.start, seg2.end};
		for(int i = 0; i < 2; i++)
			for(int j = 0; j < 2; j++)
				if(distance(p1[i], p2[j]) < constant::epsilon)
					return make_pair(p1[i], true);
		return make_pair(float2(), false);
	}

	float t = -proj_seg2.start.y / sdir.y;
	float2 p = proj_seg2.start + sdir * t;
	return make_pair(dir * p.x + nrm * p.y + seg1.start, true);
}

pair<float2, bool> intersection(const Segment2D &seg1, const Segment2D &seg2) {
	auto result = lineIntersection(seg1, seg2);
	if(result.second) {
		float2 dir1 = (seg1.end - seg1.start);
		float2 dir2 = (seg2.end - seg2.start);
		float seg1_pos = dot(result.first - seg1.start, dir1);
		float seg2_pos = dot(result.first - seg2.start, dir2);

		if(seg1_pos < 0.0f || seg1_pos > dot(dir1, dir1) || seg2_pos < 0.0f ||
		   seg2_pos > dot(dir2, dir2))
			return make_pair(float2(), false);
	}

	return result;
}

// TODO: simplify this and lineIntersections (too many special cases and epsilons)
ClipResult clip(const Triangle2D &tri, const Segment2D &iseg) {
	Segment2D seg = iseg;
	ClipResult out;

	for(int i = 0; i < 3; i++) {
		float2 a = tri[i], b = tri[(i + 1) % 3], c = tri[(i + 2) % 3];
		float2 dir = normalize(b - a), nrm(-dir.y, dir.x);
		float mul = dot(c - a, nrm) < 0.0f ? -1.0f : 1.0f;

		float dot1 = dot(seg.start - a, nrm) * mul, dot2 = dot(seg.end - a, nrm) * mul;
		if(dot1 <= 0.0f && dot2 <= 0.0f)
			return ClipResult();
		if(dot1 >= 0.0f && dot2 >= 0.0f)
			continue;

		if(seg.empty())
			return ClipResult();
		auto isect = lineIntersection(seg, Segment2D(a, b));
		if(!isect.second) {
			return ClipResult();
			xmlPrint("(%) (%) (%)   : (%) (%)\n", tri[0], tri[1], tri[2], iseg.start, iseg.end);
		}
		DASSERT(isect.second);

		if(dot1 <= 0.0f) {
			Segment2D outside(seg.start, isect.first);
			seg = Segment2D(isect.first, seg.end);
			if(out.outside_front.empty()) {
				out.outside_front = outside;
			} else {
				DASSERT(out.outside_front.end == outside.start);
				out.outside_front.end = outside.end;
			}
		} else {
			Segment2D outside(isect.first, seg.end);
			seg = Segment2D(seg.start, isect.first);
			if(out.outside_back.empty()) {
				out.outside_back = outside;
			} else {
				DASSERT(out.outside_back.start == outside.end);
				out.outside_back.start = outside.start;
			}
		}
	}

	// TODO: compute outside segments
	out.inside = seg;
	return out;
}
}
