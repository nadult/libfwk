// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// dot(plane.normal(), pointOnPlane) == plane.distance();
template <class T, int N> class Plane {
  public:
	static_assert(is_fpt<T>);
	enum { dim_size = N };

	using Vec = MakeVec<T, N>;
	using Point = Vec;
	using Scalar = T;
	using Ray = fwk::Ray<T, N>;
	using Triangle = fwk::Triangle<T, N>;

	Plane() { m_normal[dim_size - 1] = T(1); }
	Plane(const Vec &normal, T distance) : m_normal(normal), m_distance0(distance) {
		DASSERT(isNormalized(normal));
	}
	Plane(const Vec &normal, const Point &point)
		: m_normal(normal), m_distance0(dot(point, normal)) {
		DASSERT(isNormalized(normal));
	}

	// TODO: should triangle be CW or CCW?
	// TODO: CCW triangle should point up?
	ENABLE_IF_SIZE(3) explicit Plane(const Triangle &);
	ENABLE_IF_SIZE(3)
	Plane(const Point &a, const Point &b, const Point &c) : Plane(Triangle(a, b, c)) {}

	const Vec &normal() const { return m_normal; }

	// distance from plane to point (0, 0, 0)
	// TODO: better name
	T distance0() const { return m_distance0; }

	T signedDistance(const Point &) const;
	Point closestPoint(const Point &point) const;

	// TODO: line would be better here...
	ENABLE_IF_SIZE(3) Maybe<Ray> isect(const Plane &) const;

	enum SideTestResult {
		all_negative = -1,
		both_sides = 0,
		all_positive = 1,
	};
	SideTestResult sideTest(CSpan<Point>) const;
	Plane operator-() const { return Plane(-m_normal, -m_distance0); }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Plane, m_normal, m_distance0);

  private:
	Vec m_normal;
	T m_distance0;
};

#undef ENABLE_IF_SIZE
}
