/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

Plane::Plane(const float3 &a, const float3 &b, const float3 &c) {
	m_nrm = normalize(cross(c - a, b - a));
	m_dist = dot(a, m_nrm);
}

const Plane normalize(const Plane &plane) {
	float mul = 1.0f / length(plane.normal());
	return Plane(plane.normal() * mul, plane.distance() * mul);
}

const Plane operator*(const Matrix4 &m, const Plane &p) {
	float4 tmp = m * float4(p.normal(), -p.distance());
	return Plane(tmp.xyz(), -tmp.w);
}

float intersection(const Ray &ray, const Plane &plane) {
	return -dot(plane, ray.origin()) / dot(plane.normal(), ray.dir());
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
