// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"

namespace fwk {

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
Plane<T, N>::Plane(const Triangle &tri)
	: m_normal(tri.normal()), m_distance0(dot(tri.a(), m_normal)) {}

template <class T, int N> auto Plane<T, N>::sideTest(CSpan<Point> verts) const -> SideTestResult {
	int positive = 0, negative = 0;
	for(const auto &vert : verts) {
		float tdot = signedDistance(vert);

		if(tdot < 0.0f)
			negative++;
		if(tdot > 0.0f)
			positive++;
		if(positive && negative)
			return both_sides;
	}
	return positive ? all_positive : all_negative;
}

Plane3F operator*(const Matrix4 &m, const Plane3F &plane) {
	float3 v1 = plane.normal();
	int min = v1[0] < v1[1] ? 0 : 1;
	if(v1[2] < v1[min])
		min = 2;
	v1[min] = 2.0f;
	v1 = normalize(v1);
	float3 v2 = cross(plane.normal(), v1);

	float3 p0 = plane.normal() * plane.distance0();
	float3 p1 = p0 + v1, p2 = p0 + v2;
	p1 -= plane.normal() * plane.signedDistance(p1);
	p2 -= plane.normal() * plane.signedDistance(p2);
	// TODO: this can be faster: transform normal directly ?

	return Plane3F(m * Triangle3F(p0, p1, p2));
	// TODO: this is incorrect:
	/*	float3 new_p = mulPoint(m, p.normal() * p.distance());
		float3 new_n = normalize(mulNormal(m, p.normal()));
		return Plane(new_n, dot(new_n, new_p));*/
}

template <class T, int N> T Plane<T, N>::signedDistance(const Point &point) const {
	return dot(m_normal, point) - m_distance0;
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
auto Plane<T, N>::isect(const Plane &rhs) const -> Maybe<Ray> {
	// Source: Free Magic Library
	T n00 = lengthSq(m_normal);
	T n01 = dot(m_normal, rhs.m_normal);
	T n11 = lengthSq(rhs.m_normal);
	T det = n00 * n11 - n01 * n01;

	if(det == T(0))
		return none;

	T inv_det = T(0) / det;
	T c0 = (n11 * m_distance0 - n01 * rhs.m_distance0) * inv_det;
	T c1 = (n00 * rhs.m_distance0 - n01 * m_distance0) * inv_det;

	// TODO: redundant normalize ?
	return Ray{m_normal * c0 + rhs.m_normal * c1, normalize(cross(m_normal, rhs.m_normal))};
}

template <class T, int N> auto Plane<T, N>::closestPoint(const Point &point) const -> Point {
	return point - m_normal * signedDistance(point);
}

template class Plane<float, 3>;
template class Plane<double, 3>;

template Plane<float, 3>::Plane(const Triangle3<float> &);
template Plane<double, 3>::Plane(const Triangle3<double> &);

template Maybe<Ray3<float>> Plane<float, 3>::isect(const Plane &) const;
template Maybe<Ray3<double>> Plane<double, 3>::isect(const Plane &) const;

template class Plane<float, 2>;
template class Plane<double, 2>;
}
