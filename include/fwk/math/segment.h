// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"
#include "fwk/math/rational.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// Results are exact only when computing on integers
template <class T, int N> class Segment {
  public:
	static_assert(is_integral<T> || is_real<T>, "");
	static_assert(N >= 2 && N <= 3, "");
	static constexpr int dim_size = N;

	using Vec = MakeVec<T, N>;
	using Scalar = T;
	using Point = Vec;
	using Isect = Variant<None, Point, Segment>;
	using IsectParam = fwk::IsectParam<T>;

	// Promotion works only for integrals
	using PT = PromoteIntegral<T>;
	using PPT = PromoteIntegral<PT>;
	using PRT = Conditional<is_integral<T>, Rational<PT>, T>;
	using PPRT = Conditional<is_integral<T>, Rational<PPT>, T>;
	using PVec = PromoteIntegral<Vec>;
	using PPVec = PromoteIntegral<PVec>;
	using PRVec =
		Conditional<is_integral<T>, Conditional<N == 2, Rational2<PT>, Rational3<PT>>, Vec>;
	using PPRVec =
		Conditional<is_integral<T>, Conditional<N == 2, Rational2<PPT>, Rational3<PPT>>, Vec>;
	using PRIsectParam = fwk::IsectParam<PRT>;
	using PPRIsectParam = fwk::IsectParam<PPRT>;

	using PReal = Conditional<is_integral<T>, double, T>;
	using PRealVec = Conditional<is_integral<T>, MakeVec<double, N>, Vec>;

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

	bool empty() const { return from == to; }

	template <class U = T, EnableIfReal<U>...> Maybe<Ray<T, N>> asRay() const;

	PReal length() const { return std::sqrt((PReal)lengthSq()); }
	PT lengthSq() const { return fwk::distanceSq<PVec>(from, to); }

	// 0.0 -> from; 1.0 -> to
	// TODO: do this properly
	template <class U> PRealVec at(U param) const {
		return PRealVec(from) + PRealVec(to - from) * PReal(param);
	}
	// TODO: at with rational args

	template <class U> Segment<PReal, N> subSegment(Interval<U> interval) const {
		return {at(interval.min), at(interval.max)};
	}

	PPRT distanceSq(const Point &) const;
	PReal distanceSq(const Segment &) const;

	PReal distance(const Point &point) const { return std::sqrt((PReal)distanceSq(point)); }
	PReal distance(const Segment &seg) const { return std::sqrt((PReal)distanceSq(seg)); }

	bool adjacent(const Point &point) const { return isOneOf(point, from, to); }
	bool adjacent(const Segment &rhs) const { return adjacent(rhs.from) || adjacent(rhs.to); }

	Isect at(const IsectParam &) const;

	PRIsectParam isectParam(const Segment &) const;
	pair<PPRIsectParam, bool> isectParam(const Triangle<T, N> &) const;
	PRIsectParam isectParam(const Box<Vec> &) const;
	template <class U = T, EnableIfReal<U>...> IsectParam isectParam(const Plane<T, N> &) const;

	Isect isect(const Segment &segment) const;
	Isect isect(const Box<Vec> &box) const;

	IsectClass classifyIsect(const Segment &) const;
	IsectClass classifyIsect(const Point &) const;
	bool testIsect(const Box<Vec> &) const;

	PRT closestPointParam(const Point &) const;
	PPRT closestPointParam(const Segment &) const;
	pair<PPRT, PPRT> closestPointParams(const Segment &) const;

	PRealVec closestPoint(const Point &pt) const;
	PRealVec closestPoint(const Segment &) const;
	pair<PRealVec, PRealVec> closestPoints(const Segment &rhs) const;

	ENABLE_IF_SIZE(3) Segment<T, 2> xz() const { return {from.xz(), to.xz()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> xy() const { return {from.xy(), to.xy()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> yz() const { return {from.yz(), to.yz()}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Segment, from, to)

	Point from, to;
};

#undef ENABLE_IF_SIZE

template <class T, int N> Box<MakeVec<T, N>> enclose(const Segment<T, N> &seg) {
	return {vmin(seg.from, seg.to), vmax(seg.from, seg.to)};
}
}
