// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"
#include "fwk/math/rational.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// Results are exact only when computing on integers
// TODO: makes no sense if we want to use rationals as base...
template <class T, int N> class Segment {
  public:
	static_assert(is_scalar<T>);

	// TODO: check if it works for rationals...
	// TODO: passing rational as T to rational should eratse it
	static_assert(N >= 2 && N <= 3);
	static constexpr int dim_size = N;

	using Vec = MakeVec<T, N>;
	using Scalar = T;
	using Point = Vec;
	using Isect = Variant<None, Point, Segment>;
	using IsectParam = fwk::IsectParam<T>;

	template <class U> using Promote = If<is_fpt<T>, U, fwk::Promote<U>>;

	// Promotion works only for integrals
	// TODO: this is a mess, clean it up
	using PT = Promote<T>;
	using PPT = Promote<PT>;

	using PRT = If<!is_fpt<T>, Rational<PT>, PT>;
	using PPRT = If<!is_fpt<T>, Rational<PPT>, PPT>;
	using PVec = MakeVec<PT, N>;
	using PPVec = MakeVec<PPT, N>;
	using PRVec = If<!is_fpt<T>, If<N == 2, Rational2<PT>, Rational3<PT>>, Vec>;
	using PPRVec = If<!is_fpt<T>, If<N == 2, Rational2<PPT>, Rational3<PPT>>, Vec>;

	using PRIsectParam = fwk::IsectParam<PRT>;
	using PPRIsectParam = fwk::IsectParam<PPRT>;

	using PReal = If<!is_fpt<T>, double, T>;
	using PRealVec = If<!is_fpt<T>, MakeVec<double, N>, Vec>;

	Segment() : from(), to() {}
	Segment(const Point &a, const Point &b) : from(a), to(b) {}
	Segment(const Pair<Point> &pair) : Segment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit Segment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit Segment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class U, EnableIf<precise_conversion<U, T>>...>
	Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}
	template <class U, EnableIf<!precise_conversion<U, T>>...>
	explicit Segment(const Segment<U, N> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}

	bool empty() const { return from == to; }
	Vec dir() const { return to - from; }

	template <class U = T, EnableIfFpt<U>...> Maybe<Ray<T, N>> asRay() const;

	Segment twin() const { return {to, from}; }

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
	Pair<PPRIsectParam, bool> isectParam(const Triangle<T, N> &) const;
	PRIsectParam isectParam(const Box<Vec> &) const;
	template <class U = T, EnableIfFpt<U>...> IsectParam isectParam(const Plane<T, N> &) const;

	Isect isect(const Segment &segment) const;
	Isect isect(const Box<Vec> &box) const;

	IsectClass classifyIsect(const Segment &) const;
	IsectClass classifyIsect(const Point &) const;
	bool testIsect(const Box<Vec> &) const;

	PRT closestPointParam(const Point &) const;
	PPRT closestPointParam(const Segment &) const;
	Pair<PPRT> closestPointParams(const Segment &) const;

	PRealVec closestPoint(const Point &pt) const;
	PRealVec closestPoint(const Segment &) const;
	Pair<PRealVec> closestPoints(const Segment &rhs) const;

	ENABLE_IF_SIZE(3) Segment<T, 2> xz() const { return {from.xz(), to.xz()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> xy() const { return {from.xy(), to.xy()}; }
	ENABLE_IF_SIZE(3) Segment<T, 2> yz() const { return {from.yz(), to.yz()}; }

	Segment operator*(const Vec &vec) const { return {from * vec, to * vec}; }
	Segment operator*(T scalar) const { return {from * scalar, to * scalar}; }
	Segment operator+(const Vec &vec) const { return {from + vec, to + vec}; }
	Segment operator-(const Vec &vec) const { return {from - vec, to - vec}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Segment, from, to)

	Point from, to;
};

#undef ENABLE_IF_SIZE

template <class T, int N> Box<MakeVec<T, N>> enclose(const Segment<T, N> &seg) {
	return {vmin(seg.from, seg.to), vmax(seg.from, seg.to)};
}
}
