// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/maybe.h"
#include "fwk/math/segment.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

template <class T, int N> class Triangle {
  public:
	enum { dim_size = N };
	static_assert(dim_size >= 2 && dim_size <= 3, "Only 2D & 3D triangles are supported");

	using Vector = MakeVec<T, N>;
	using Point = Vector;
	using Scalar = T;
	using Segment = fwk::Segment<T, N>;
	using Ray = fwk::Ray<T, N>;
	using Box = fwk::Box<Vector>;

	Triangle(const Point &a, const Point &b, const Point &c) : v{a, b, c} {}
	Triangle() = default;

	template <class VT>
	explicit Triangle(const Triangle<VT, N> &rhs)
		: v{Vector(rhs[0]), Vector(rhs[1]), Vector(rhs[2])} {}

	bool degenerate() const { return v[0] == v[1] || v[1] == v[2] || v[2] == v[0]; }
	bool isPoint() const { return v[0] == v[1] && v[0] == v[2]; }
	bool isSegment() const { return degenerate() && !isPoint(); }

	bool adjacent(const Triangle &rhs) const {
		return anyOf(v, [&](auto v) { return isOneOf(v, rhs.points()); });
	}

	// Edge1: b - a;  Edge2: c - a

	Point a() const { return v[0]; }
	Point b() const { return v[1]; }
	Point c() const { return v[2]; }

	Segment ab() const { return {v[0], v[1]}; }
	Segment bc() const { return {v[1], v[2]}; }
	Segment ca() const { return {v[2], v[0]}; }
	Segment edge(int idx) const {
		PASSERT(idx >= 0 && idx <= 3);
		return {v[idx], v[idx == 2 ? 0 : idx + 1]};
	}

	Point center() const { return (v[0] + v[1] + v[2]) * (T(1) / T(3)); }

	Triangle flipped() const { return Triangle(v[2], v[1], v[0]); }

	ENABLE_IF_SIZE(3) Triangle2<T> xz() const { return {v[0].xz(), v[1].xz(), v[2].xz()}; }
	ENABLE_IF_SIZE(3) Triangle2<T> xy() const { return {v[0].xy(), v[1].xy(), v[2].xy()}; }
	ENABLE_IF_SIZE(3) Triangle2<T> yz() const { return {v[0].yz(), v[1].yz(), v[2].yz()}; }
	ENABLE_IF_SIZE(3) Triangle2<T> projection2D() const;

	ENABLE_IF_SIZE(3) Vector normal() const;
	ENABLE_IF_SIZE(3) Vector barycentric(const Point &point) const;
	vector<Point> sampleEven(float density) const;

	T surfaceArea() const;

	CSpan<Point, 3> points() const { return {v}; }

	auto edges() const {
		return indexRange(0, 3, [&](int i) { return edge(i); });
	}
	array<T, 3> angles() const;

	const Point &operator[](int idx) const { return v[idx]; }

	T distance(const Point &) const;
	Point closestPoint(const Point &) const;

	ENABLE_IF_SIZE(3) Maybe<Segment> isect(const Triangle &) const;

	bool areIntersecting(const Triangle &) const;
	ENABLE_IF_SIZE(3) bool testIsect(const Box &) const;

	FWK_ORDER_BY(Triangle, v[0], v[1], v[2]);

  private:
	Point v[3];
};

#undef ENABLE_IF_SIZE

template <class T, int N> Box<MakeVec<T, N>> enclose(const Triangle<T, N> &tri) {
	return enclose(tri.points());
}
}
