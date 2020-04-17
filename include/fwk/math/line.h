// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/isect_param.h"
#include "fwk/math/rational.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// Results are exact only when computing on integers
template <class TVec> class Line {
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
	using Isect = Variant<None, Point, Line>;
	using IsectParam = fwk::IsectParam<T>;

	template <class U> using Promote = If<is_fpt<T>, U, fwk::Promote<U>>;

	// Promotion works only for integrals
	// TODO: this is a mess, clean it up
	using PT = Promote<T>;
	using PPT = Promote<PT>;

	using PRT = MakeRat<PT>;
	using PPRT = MakeRat<PPT>;
	using PVec = MakeVec<PT, dim>;
	using PPVec = MakeVec<PPT, dim>;

	using PRIsectParam = fwk::IsectParam<PRT>;
	using PPRIsectParam = fwk::IsectParam<PPRT>;

	using PReal = If<!is_fpt<T>, double, T>;
	using PRealVec = If<!is_fpt<T>, MakeVec<double, dim>, Vec>;

	Line() : origin(), dir(1) {}
	Line(const Point &origin, const Vec &dir) : origin(origin), dir(dir) { DASSERT(valid()); }
	Line(const Pair<Vec> &pair) : Line(pair.first, pair.second) {}

	template <class UVec, EnableIf<precise_conversion<UVec, TVec>>...>
	Line(const Line<UVec> &rhs) : Line(Vec(rhs.origin), Vec(rhs.dir)) {}
	template <class UVec, EnableIf<!precise_conversion<UVec, TVec>>...>
	explicit Line(const Line<UVec> &rhs) : Line(Vec(rhs.origin), Vec(rhs.dir)) {}

	bool valid() const { return dir != Vec(); }

	// TODO: do this properly (with rationals)
	template <class U> PRealVec at(U param) const {
		return PRealVec(origin) + PRealVec(dir) * PReal(param);
	}
	// TODO: at with rational args

	template <class U> Segment<PRealVec> subSegment(Interval<U> interval) const {
		return {at(interval.min), at(interval.max)};
	}

	PRIsectParam isectParam(const Line &) const;
	// TODO: isectParam with segment

	PRT closestPointParam(const Point &) const;

	ENABLE_IF_SIZE(3) Line<MakeVec<T, 2>> xz() const { return {origin.xz(), dir.xz()}; }
	ENABLE_IF_SIZE(3) Line<MakeVec<T, 2>> xy() const { return {origin.xy(), dir.xy()}; }
	ENABLE_IF_SIZE(3) Line<MakeVec<T, 2>> yz() const { return {origin.yz(), dir.yz()}; }

	Line operator*(const Vec &vec) const { return {origin * vec, dir * vec}; }
	Line operator*(T scalar) const { return {origin * scalar, dir * scalar}; }
	Line operator+(const Vec &vec) const { return {origin + vec, dir + vec}; }
	Line operator-(const Vec &vec) const { return {origin - vec, dir - vec}; }

	void operator>>(TextFormatter &) const;

	FWK_ORDER_BY(Line, origin, dir)

	Vec origin, dir;
};

#undef ENABLE_IF_SIZE

}
