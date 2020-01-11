// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/plane.h"

#include "fwk/math/ray.h"
#include "fwk/math/triangle.h"

namespace fwk {

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
Plane<T, N>::Plane(const Triangle &tri) {
	DASSERT(!tri.degenerate());
	m_normal = tri.normal();
	m_distance0 = dot(tri.a(), m_normal);
}

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

	T inv_det = T(1) / det;
	T c0 = (n11 * m_distance0 - n01 * rhs.m_distance0) * inv_det;
	T c1 = (n00 * rhs.m_distance0 - n01 * m_distance0) * inv_det;

	// TODO: redundant normalize ?
	return Ray{m_normal * c0 + rhs.m_normal * c1, normalize(cross(m_normal, rhs.m_normal))};
}

template <class T, int N> auto Plane<T, N>::closestPoint(const Point &point) const -> Point {
	return point - m_normal * signedDistance(point);
}

template <class T, int N> void Plane<T, N>::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%; %)" : "% %", m_normal, m_distance0);
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
