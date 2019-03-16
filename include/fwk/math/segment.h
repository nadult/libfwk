// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"
#include "fwk/math/rational.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// Results are exact only when computing on integers
template <class TVec> class Segment {
  public:
	static_assert(is_vec<TVec>);

	// When computing on integers, you need 2x as many bits to represent 2D segment intersection
	// With rationals it's 4x as much (rational addition / subtraction in general case requires multiplication)
	static_assert(!is_rational<TVec>, "Complex computations on rationals are not supported");

	static constexpr int dim = fwk::dim<TVec>;
	static_assert(dim >= 2 && dim <= 3);

	using Vec = TVec;
	using T = fwk::Scalar<TVec>;
	using Point = Vec;
	using Isect = Variant<None, Point, Segment>;
	using IsectParam = fwk::IsectParam<T>;

	template <class U> using Promote = If<is_fpt<T>, U, fwk::Promote<U>>;

	// Promotion works only for integrals
	// TODO: this is a mess, clean it up
	using PT = Promote<T>;
	using PPT = Promote<PT>;

	// TODO: computations on rationals can overflow (I didn't promote when adding/subtracting)
	using PRT = MakeRat<PT>;
	using PPRT = MakeRat<PPT>;
	using PVec = MakeVec<PT, dim>;
	using PPVec = MakeVec<PPT, dim>;
	using PRVec = MakeRat<PT, dim>;
	using PPRVec = MakeRat<PPT, dim>;

	using PRIsectParam = fwk::IsectParam<PRT>;
	using PPRIsectParam = fwk::IsectParam<PPRT>;

	using PReal = If<!is_fpt<T>, double, T>;
	using PRealVec = If<!is_fpt<T>, MakeVec<double, dim>, Vec>;

	Segment() : from(), to() {}
	Segment(const Point &a, const Point &b) : from(a), to(b) {}
	Segment(const Pair<Point> &pair) : Segment(pair.first, pair.second) {}

	ENABLE_IF_SIZE(2) explicit Segment(T x1, T y1, T x2, T y2) : from(x1, y1), to(x2, y2) {}
	ENABLE_IF_SIZE(3)
	explicit Segment(T x1, T y1, T z1, T x2, T y2, T z2) : from(x1, y1, z1), to(x2, y2, z2) {}

	template <class UVec, EnableIf<precise_conversion<UVec, TVec>>...>
	Segment(const Segment<UVec> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}
	template <class UVec, EnableIf<!precise_conversion<UVec, TVec>>...>
	explicit Segment(const Segment<UVec> &rhs) : Segment(Point(rhs.from), Point(rhs.to)) {}

	bool empty() const { return from == to; }
	Vec dir() const { return to - from; }

	template <class U = T, EnableIfFpt<U>...> Maybe<Ray<T, dim>> asRay() const;

	Segment twin() const { return {to, from}; }

	PReal length() const { return std::sqrt((PReal)lengthSq()); }
	PT lengthSq() const { return fwk::distanceSq<PVec>(from, to); }

	// from + dir() * param
	template <class U, EnableIf<is_scalar<U>>...> auto at(U param) const {
		if constexpr(is_integral<Base<U>>)
			static_assert(precise_conversion<T, U>);
		using OVec = MakeVec<Promote<U>, dim>;

		if constexpr(is_rational<U>)
			return ratDivide(OVec(from) * param.den() + OVec(dir()) * param.num(), param.den());
		else
			return OVec(from) + OVec(dir()) * param;
	}

	template <class U> Segment<PRealVec> subSegment(Interval<U> interval) const {
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
	Pair<PPRIsectParam, bool> isectParam(const Triangle<T, dim> &) const;
	PRIsectParam isectParam(const Box<Vec> &) const;

	// TODO: gcc is causing compilation problems...
	template <class U = T, EnableIfFpt<U>...>
	IsectParam isectParamPlane(const Plane<T, dim> &) const;

	Isect isect(const Segment &segment) const;
	Isect isect(const Box<Vec> &box) const;

	IsectClass classifyIsect(const Segment &) const;
	IsectClass classifyIsect(const Point &) const;
	bool testIsect(const Box<Vec> &) const;

	PRT closestPointParam(const Point &) const;
	PPRT closestPointParam(const Segment &) const;
	Pair<PPRT> closestPointParams(const Segment &) const;

	auto closestPoint(const Point &pt) const { return at(closestPointParam(pt)); }
	auto closestPoint(const Segment &seg) const { return at(closestPointParam(seg)); }
	auto closestPoints(const Segment &rhs) const {
		auto params = closestPointParams(rhs);
		return pair{at(params.first), rhs.at(params.second)};
	}

	ENABLE_IF_SIZE(3) Segment<MakeVec<T, 2>> xz() const { return {from.xz(), to.xz()}; }
	ENABLE_IF_SIZE(3) Segment<MakeVec<T, 2>> xy() const { return {from.xy(), to.xy()}; }
	ENABLE_IF_SIZE(3) Segment<MakeVec<T, 2>> yz() const { return {from.yz(), to.yz()}; }

	Segment operator*(const Vec &vec) const { return {from * vec, to * vec}; }
	Segment operator*(T scalar) const { return {from * scalar, to * scalar}; }
	Segment operator+(const Vec &vec) const { return {from + vec, to + vec}; }
	Segment operator-(const Vec &vec) const { return {from - vec, to - vec}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Segment, from, to)

	Point from, to;
};

#undef ENABLE_IF_SIZE

template <class TVec> Box<TVec> enclose(const Segment<TVec> &seg) {
	return {vmin(seg.from, seg.to), vmax(seg.from, seg.to)};
}
}
