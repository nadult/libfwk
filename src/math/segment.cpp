/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_variant.h"
#include "fwk_xml.h"
#include <cmath>
#include <cstdlib>

namespace fwk {

template <class Real, int N>
auto Segment<Real, N>::closestPoint(const Point &point) const -> PPoint {
	if(empty())
		return {from, Real(0)};

	auto vec = to - from;
	auto t = dot(point - from, vec) / fwk::lengthSq(vec);
	t = clamp(t, Scalar(0), Scalar(1));
	return {from + vec * t, t};
}

template <class Real, int N>
auto Segment<Real, N>::closestPoint(const Segment &seg) const -> PPoint {
	return closestPoints(seg).first;
}

// Source: Real-time collision detection (page 150)
template <class Real, int N>
auto Segment<Real, N>::closestPoints(const Segment &rhs) const -> pair<PPoint, PPoint> {
	if(empty()) {
		auto point2 = rhs.closestPoint(from);
		return {PPoint{from, Real(0)}, point2};
	}

	auto r = from - rhs.from;
	auto d1 = to - from, d2 = rhs.to - rhs.from;
	auto a = dot(d1, d1), e = dot(d2, d2);
	auto f = dot(d2, r);

	auto c = dot(d1, r);
	auto b = dot(d1, d2);
	auto denom = a * e - b * b;

	Real s = 0, t = 0;

	if(denom != Real(0)) {
		s = clamp((b * f - c * e) / denom, Real(0), Real(1));
	} else {
		// TODO: handle it properly
		s = 0;
	}

	t = (b * s + f) / e;

	if(t < Real(0)) {
		t = 0;
		s = clamp(-c / a, Real(0), Real(1));
	} else if(t > Real(1)) {
		t = 1;
		s = clamp((b - c) / a, Real(0), Real(1));
	}

	return make_pair(PPoint{at(s), s}, PPoint{rhs.at(t), t});
}

// Source: Real-time collision detection (page 130)
template <class Real, int N> Real Segment<Real, N>::distanceSq(const Point &point) const {
	// TODO: verify this
	auto ab = to - from, ac = point - from, bc = point - to;
	auto e = dot(ac, ab);
	if(e <= Scalar(0))
		return dot(ac, ac);
	auto f = dot(ab, ab);
	if(e >= f)
		return dot(bc, bc);
	return dot(ac, ac) - e * e / f;
}

template <class Real, int N> Real Segment<Real, N>::distanceSq(const Segment &rhs) const {
	auto points = closestPoints(rhs);
	return fwk::distanceSq(points.first.point, points.second.point);
}

template <class R> static Segment2Isect<R> isect(const Segment2<R> &seg1, const Segment2<R> &seg2) {
	if(seg1.empty()) {
		if(seg2.empty()) {
			if(seg1.to == seg2.to)
				return seg1.to;
			return none;
		}
		return isect(seg2, seg1);
	}

	auto vec1 = seg1.to - seg1.from;
	auto vec2 = seg2.to - seg2.from;
	R length_sq = lengthSq(vec1);

	auto tdot = dot(vec1, perpendicular(vec2));
	if(tdot == R(0)) {
		// lines are parallel
		if(dot(seg2.from - seg1.from, perpendicular(vec2)) == R(0)) {
			// lines are the same
			R t1 = dot(seg2.from - seg1.from, vec1) / length_sq;
			R t2 = dot(seg2.to - seg1.from, vec1) / length_sq;

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, R(0));
			t2 = min(t2, R(1));

			if(t2 < R(0) || t1 > R(1))
				return none;
			if(t1 == t2)
				return seg1.from + vec1 * t1;
			return Segment2<R>(seg1.from + vec1 * t1, seg1.from + vec1 * t2);
		}

		return none;
	}

	auto diff = seg2.from - seg1.from;
	auto idot = R(1) / tdot;
	auto t1 = dot(diff, perpendicular(vec2)) * idot;
	auto t2 = dot(diff, perpendicular(vec1)) * idot;
	auto point = seg1.from + t1 * vec1;

	if(t1 >= R(0) && t1 <= R(1) && t2 >= R(0) && t2 <= R(1))
		return point;
	return none;
}

Segment2Isect<float> intersection(const Segment2<float> &lhs, const Segment2<float> &rhs) {
	return isect(lhs, rhs);
}

Segment2Isect<double> intersection(const Segment2<double> &lhs, const Segment2<double> &rhs) {
	return isect(lhs, rhs);
}

template struct Segment<float, 2>;
template struct Segment<float, 3>;
template struct Segment<double, 2>;
template struct Segment<double, 3>;

// TODO: proper intersections (not based on rays)
pair<float, float> intersectionRange(const Segment<float, 3> &segment, const Box<float3> &box) {
	DASSERT(!segment.empty());

	auto len = segment.length();
	auto pair = intersectionRange(*segment.asRay(), box);
	pair.first /= len;
	pair.second /= len;

	pair.second = min(pair.second, 1.0f);
	return pair.first <= pair.second ? pair : make_pair(fconstant::inf, fconstant::inf);
}

float intersection(const Segment<float, 3> &segment, const Triangle &tri) {
	DASSERT(!segment.empty());

	float isect = intersection(*segment.asRay(), tri) / segment.length();
	return isect < 0.0f || isect > 1.0f ? fconstant::inf : isect;
}

float intersection(const Segment<float, 3> &segment, const Plane &plane) {
	DASSERT(!segment.empty());
	float dist = intersection(*segment.asRay(), plane) / segment.length();
	return dist < 0.0f || dist > 1.0f ? fconstant::inf : dist;
}

Segment<float, 3> operator*(const Matrix4 &mat, const Segment<float, 3> &segment) {
	return {mulPoint(mat, segment.from), mulPoint(mat, segment.to)};
}
}
