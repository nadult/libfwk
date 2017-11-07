// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

template <class T, int N> class Ray {
  public:
	static_assert(isReal<T>(), "Ray cannot be constructed using integral numbers as base type");
	using Scalar = T;
	using Vector = MakeVec<T, N>;
	using Point = Vector;
	using Segment = fwk::Segment<T, N>;
	using IsectParam = fwk::IsectParam<T>;
	using Isect = Variant<None, Point, Segment>;

	Ray(const Vector &origin, const Vector &dir) : m_origin(origin), m_dir(dir) {
		DASSERT(!isZero(m_dir));
		DASSERT(isNormalized(m_dir));
	}

	const Vector &dir() const { return m_dir; }
	const Point &origin() const { return m_origin; }
	auto invDir() const { return vinv(m_dir); }
	const Point at(T t) const { return m_origin + m_dir * t; }

	// TODO: should we allow empty rays?
	bool empty() const { return isZero(m_dir); }

	T distance(const Point &) const;
	T distance(const Ray &) const;

	T closestPointParam(const Point &) const;
	Point closestPoint(const Point &) const;
	pair<T, T> closestPointsParam(const Ray &) const;
	pair<Point, Point> closestPoints(const Ray &) const;

	IsectParam isectParam(const Box<Vector> &) const;
	ENABLE_IF_SIZE(3) IsectParam isectParam(const Plane<T, N> &) const;
	ENABLE_IF_SIZE(3) IsectParam isectParam(const Triangle<T, N> &) const;

	FWK_ORDER_BY(Ray, m_origin, m_dir);

  private:
	Point m_origin;
	Vector m_dir;
};

#undef ENABLE_IF_SIZE
}