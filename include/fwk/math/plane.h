// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

// dot(plane.normal(), pointOnPlane) == plane.distance();
template <class T, int N> class Plane {
  public:
	static_assert(isReal<T>(), "Plane<> should be based on reals");
	enum { dim_size = N };

	using Vector = MakeVec<T, N>;
	using Point = Vector;
	using Scalar = T;
	using Segment = fwk::Segment<T, N>;
	using Ray = fwk::Ray<T, N>;
	using Triangle = fwk::Triangle<T, N>;

	Plane() { m_normal[dim_size - 1] = T(1); }
	Plane(const Vector &normal, T distance) : m_normal(normal), m_distance0(distance) {
		DASSERT(isNormalized(normal));
	}
	Plane(const Vector &normal, const Point &point)
		: m_normal(normal), m_distance0(dot(point, normal)) {
		DASSERT(isNormalized(normal));
	}

	// TODO: should triangle be CW or CCW?
	// TODO: CCW triangle should point up?
	ENABLE_IF_SIZE(3) explicit Plane(const Triangle &);
	ENABLE_IF_SIZE(3)
	Plane(const Point &a, const Point &b, const Point &c) : Plane(Triangle(a, b, c)) {}

	const Vector &normal() const { return m_normal; }

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

	FWK_ORDER_BY(Plane, m_normal, m_distance0);

  private:
	Vector m_normal;
	T m_distance0;
};

#undef ENABLE_IF_SIZE
}
