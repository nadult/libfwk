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

template <class T, int N> auto Ray<T, N>::closestPointParam(const Point &point) const -> T {
	return dot(point - m_origin, m_dir);
}

template <class T, int N> auto Ray<T, N>::closestPoint(const Point &point) const -> Point {
	return at(closestPointParam(point));
}

// Algorithm idea: http://www.geometrictools.com/GTEngine/Include/Mathematics/GteDistLineLine.h
template <class T, int N>
auto Ray<T, N>::closestPoints(const Ray &rhs) const -> pair<Point, Point> {
	Vector diff = m_origin - rhs.m_origin;
	T a01 = -dot(m_dir, rhs.m_dir);
	T b0 = dot(diff, m_dir);
	T s1 = -b0, s2 = 0;

	// TODO: verify this is correct; what if det == 0 ?
	if(std::abs(a01) < T(1)) {
		T det = T(1) - a01 * a01;
		T b1 = -dot(diff, rhs.m_dir);
		s1 = (a01 * b1 - b0) / det;
		s2 = (a01 * b0 - b1) / det;
	}

	return make_pair(at(s1), rhs.at(s2));
}

template <class T, int N> T Ray<T, N>::distance(const Point &point) const {
	return fwk::distance(closestPoint(point), point);
}

template <class T, int N> T Ray<T, N>::distance(const Ray &rhs) const {
	auto points = closestPoints(rhs);
	return fwk::distance(points.first, points.second);
}

template <class T, int N> IsectParam<T> Ray<T, N>::isectParam(const Box<Vector> &box) const {
	Vector inv_dir = inv(m_dir);

	T l1 = inv_dir.x * (box.x() - m_origin.x);
	T l2 = inv_dir.x * (box.ex() - m_origin.x);
	T lmin = min(l1, l2);
	T lmax = max(l1, l2);

	l1 = inv_dir.y * (box.y() - m_origin.y);
	l2 = inv_dir.y * (box.ey() - m_origin.y);
	lmin = max(lmin, min(l1, l2));
	lmax = min(lmax, max(l1, l2));

	if
		constexpr(N == 3) {
			l1 = inv_dir.z * (box.z() - m_origin.z);
			l2 = inv_dir.z * (box.ez() - m_origin.z);
			lmin = max(lmin, min(l1, l2));
			lmax = min(lmax, max(l1, l2));
		}

	if(lmin > lmax)
		return {};
	return {lmin, lmax};
}

// Moller-Trombore, source: wikipedia
template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
IsectParam<T> Ray<T, N>::isectParam(const Triangle<T, N> &tri) const {
	Vector e1 = tri[1] - tri[0], e2 = tri[2] - tri[0];

	// Begin calculating determinant - also used to calculate u parameter
	Vector vp = cross(m_dir, e2);
	T det = dot(e1, vp);

	// if determinant is near zero, ray lies in plane of triangle
	if(det > -fconstant::isect_epsilon && det < fconstant::isect_epsilon) {
		// TODO: fix this...
		return {};
	}
	T inv_det = T(1) / det;

	// calculate distance from V1 to ray origin
	Vector vt = m_origin - tri[0];

	// Calculate u parameter and test bound
	T tu = dot(vt, vp) * inv_det;
	// The intersection lies outside of the triangle
	if(tu < T(0) || tu > T(1))
		return {};

	// Prepare to test v parameter
	Vector vq = cross(vt, e1);

	// Calculate V parameter and test bound
	T tv = dot(m_dir, vq) * inv_det;
	// The intersection lies outside of the triangle
	if(tv < T(0) || tu + tv > T(1))
		return {};

	T t = dot(e2, vq) * inv_det;

	if(t > fconstant::epsilon)
		return t;

	return {};
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
IsectParam<T> Ray<T, N>::isectParam(const Plane<T, N> &plane) const {
	// TODO: fix me
	T ndot = dot(plane.normal(), m_dir);
	if(ndot == T(0))
		return fconstant::inf;

	return -plane.signedDistance(m_origin) / ndot;
}

template class Ray<float, 3>;
template class Ray<double, 3>;

template class Ray<float, 2>;
template class Ray<double, 2>;

template auto Ray<float, 3>::isectParam(const Plane<float, 3> &) const -> IsectParam;
template auto Ray<double, 3>::isectParam(const Plane<double, 3> &) const -> IsectParam;
template auto Ray<float, 3>::isectParam(const Triangle<float, 3> &) const -> IsectParam;
template auto Ray<double, 3>::isectParam(const Triangle<double, 3> &) const -> IsectParam;
}
