/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

Plane::Plane(const Triangle &tri) : m_nrm(tri.normal()), m_dist(dot(tri.a(), m_nrm)) {}

const Plane normalize(const Plane &plane) {
	float mul = 1.0f / length(plane.normal());
	return Plane(plane.normal() * mul, plane.distance() * mul);
}

const Plane operator*(const Matrix4 &m, const Plane &p) {
	float3 v1 = p.normal();
	int min = v1[0] < v1[1] ? 0 : 1;
	if(v1[2] < v1[min])
		min = 2;
	v1[min] = 2.0f;
	v1 = normalize(v1);
	float3 v2 = cross(p.normal(), v1);

	float3 p0 = p.normal() * p.distance();
	float3 p1 = p0 + v1, p2 = p0 + v2;
	p1 -= p.normal() * dot(p, p1);
	p2 -= p.normal() * dot(p, p2);

	return Plane(m * Triangle(p0, p1, p2));

	// TODO: this is incorrect:
	/*	float3 new_p = mulPoint(m, p.normal() * p.distance());
		float3 new_n = normalize(mulNormal(m, p.normal()));
		return Plane(new_n, dot(new_n, new_p));*/
}

float dot(const Plane &plane, const float3 &point) {
	return dot(plane.normal(), point) - plane.distance();
}

bool intersection(const Plane &plane0, const Plane &plane1, Ray &line) {
	// Source: Free Magic Library
	float n00 = lengthSq(plane0.normal());
	float n01 = dot(plane0.normal(), plane1.normal());
	float n11 = lengthSq(plane1.normal());
	float det = n00 * n11 - n01 * n01;

	if(fabsf(det) < constant::epsilon)
		return false;

	float inv_det = 1.0f / det;
	float c0 = (n11 * plane0.distance() - n01 * plane1.distance()) * inv_det;
	float c1 = (n00 * plane1.distance() - n01 * plane0.distance()) * inv_det;

	line = Ray(float3(plane0.normal() * c0 + plane1.normal() * c1),
			   normalize(cross(plane0.normal(), plane1.normal())));
	return true;
}
}
