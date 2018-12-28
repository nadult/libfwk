// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"
#include "fwk/math/rational.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

template <class T, int N> class ISegment {
  public:
	static_assert(isIntegral<T>(), "");
	static_assert(N >= 2 && N <= 3, "");

	using Vector = MakeVec<T, N>;
	using Scalar = T;
	using PScalar = Promote<Scalar>;
	using PPScalar = Promote<PScalar>;
	// TODO: check if PScalar has enough bits for intersections

	using Point = Vector;
	using IsectParam = fwk::IsectParam<Rational<PPScalar>>;
	enum { dim_size = N };

	ISegment() : from(), to() {}
	ISegment(const Point &a, const Point &b) : from(a), to(b) {}
	ISegment(const pair<Point, Point> &pair) : ISegment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit ISegment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit ISegment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	explicit ISegment(const ISegment<U, N> &rhs) : ISegment(Point(rhs.from), Point(rhs.to)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit ISegment(const ISegment<U, N> &rhs) : ISegment(Point(rhs.from), Point(rhs.to)) {}

	bool empty() const { return from == to; }

	bool adjacent(const Point &point) const { return isOneOf(point, from, to); }
	bool adjacent(const ISegment &rhs) const { return adjacent(rhs.from) || adjacent(rhs.to); }

	// Computations performed on qints;
	// You have to be careful if values are greater than 32 bits
	ENABLE_IF_SIZE(2) IsectClass classifyIsect(const ISegment &) const;
	ENABLE_IF_SIZE(2) IsectClass classifyIsect(const Point &) const;
	bool testIsect(const Box<Vector> &) const;

	ENABLE_IF_SIZE(3) pair<IsectParam, bool> isectParam(const Triangle3<T> &) const;

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(ISegment, from, to)

	Point from, to;
};

template <class T, int N> class Segment {
  public:
	static_assert(!isIntegral<T>(), "use ISegment for integer-based segments");
	static_assert(isReal<T>(), "");
	static_assert(N >= 2 && N <= 3, "");

	using Vector = MakeVec<T, N>;
	using Scalar = T;
	using Point = Vector;
	using IsectParam = fwk::IsectParam<T>;
	using Isect = Variant<None, Point, Segment>;
	enum { dim_size = N };

	Segment() : from(), to() {}
	Segment(const Point &a, const Point &b) : from(a), to(b) {}
	Segment(const pair<Point, Point> &pair) : Segment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit Segment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit Segment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	explicit Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}

	template <class U> explicit operator ISegment<U, N>() const {
		using IVector = MakeVec<U, N>;
		return {IVector(from), IVector(to)};
	}
	template <class U>
	explicit Segment(const ISegment<U, N> &iseg) : from(iseg.from), to(iseg.to) {}

	bool empty() const { return from == to; }

	Maybe<Ray<T, N>> asRay() const;

	auto length() const { return fwk::distance(from, to); }
	T lengthSq() const { return fwk::distanceSq(from, to); }

	// 0.0 -> from; 1.0 -> to
	Vector at(T param) const { return from + (to - from) * param; }
	Segment subSegment(Interval<T> interval) const { return {at(interval.min), at(interval.max)}; }

	T distanceSq(const Point &) const;
	T distanceSq(const Segment &) const;

	T distance(const Point &point) const { return std::sqrt(distanceSq(point)); }
	T distance(const Segment &seg) const { return std::sqrt(distanceSq(seg)); }

	bool adjacent(const Point &point) const { return isOneOf(point, from, to); }
	bool adjacent(const Segment &rhs) const { return adjacent(rhs.from) || adjacent(rhs.to); }

	Isect at(const IsectParam &) const;

	ENABLE_IF_SIZE(2) IsectParam isectParam(const Segment &) const;
	ENABLE_IF_SIZE(2) Isect isect(const Segment &segment) const { return at(isectParam(segment)); }

	ENABLE_IF_SIZE(2) IsectClass classifyIsect(const Segment &) const;
	ENABLE_IF_SIZE(2) IsectClass classifyIsect(const Point &) const;
	ENABLE_IF_SIZE(2) bool testIsect(const Box<Vector> &) const;

	ENABLE_IF_SIZE(3) IsectParam isectParam(const Triangle<T, N> &) const;
	ENABLE_IF_SIZE(3) IsectParam isectParam(const Plane<T, N> &) const;
	IsectParam isectParam(const Box<Vector> &) const;

	Isect isect(const Box<Vector> &box) const { return at(isectParam(box)); }

	T closestPointParam(const Point &) const;
	T closestPointParam(const Segment &) const;
	pair<T, T> closestPointParams(const Segment &) const;

	Vector closestPoint(const Point &pt) const;
	Vector closestPoint(const Segment &) const;
	pair<Vector, Vector> closestPoints(const Segment &rhs) const;

	ENABLE_IF_SIZE(3) Segment<T, 2> xz() const { return {from.xz(), to.xz()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> xy() const { return {from.xy(), to.xy()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> yz() const { return {from.yz(), to.yz()}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Segment, from, to)

	Point from, to;
};

namespace detail {
	template <class T, int N, EnableIfIntegral<T>...> auto makeSegment(T) -> ISegment<T, N>;
	template <class T, int N, EnableIfReal<T>...> auto makeSegment(T) -> Segment<T, N>;
}
template <class T, int N> using MakeSegment = decltype(detail::makeSegment<T, N>(T()));

template <class T, int N> Box<MakeVec<T, N>> enclose(const Segment<T, N> &seg) {
	return {vmin(seg.from, seg.to), vmax(seg.from, seg.to)};
}

template <class T, int N> Box<MakeVec<T, N>> enclose(const ISegment<T, N> &seg) {
	return {vmin(seg.from, seg.to), vmax(seg.from, seg.to)};
}

#undef ENABLE_IF_SIZE
}
