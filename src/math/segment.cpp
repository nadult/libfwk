/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_variant.h"
#include "fwk_xml.h"
#include <cmath>
#include <cstdlib>

namespace fwk {

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
SegmentIsectClass ISegment<T, N>::classifyIsect(const ISegment<T, N> &rhs) const {
	if(sharedEndPoints(rhs))
		return SegmentIsectClass::shared_endpoints;

	auto vec1 = to - from;
	auto vec2 = rhs.to - rhs.from;

	auto tdot = dot(vec1, perpendicular(vec2));
	if(tdot == T(0)) {
		// lines are parallel
		if(dot(rhs.from - from, perpendicular(vec2)) == T(0)) {
			// lines are the same
			T length_sq = lengthSq(vec1);
			T t1 = dot(rhs.from - from, vec1);
			T t2 = dot(rhs.to - from, vec1);

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, T(0));
			t2 = min(t2, T(length_sq));

			if(t2 < T(0) || t1 > length_sq)
				return SegmentIsectClass::none;
			if(t1 == t2)
				return SegmentIsectClass::point;
			return SegmentIsectClass::segment;
		}

		return SegmentIsectClass::none;
	}

	auto diff = rhs.from - from;
	auto t1 = dot(diff, perpendicular(vec2));
	auto t2 = dot(diff, perpendicular(vec1));

	if(tdot < T(0)) {
		t1 = -t1;
		t2 = -t2;
		tdot = -tdot;
	}

	if(t1 >= T(0) && t1 <= tdot && t2 >= T(0) && t2 <= tdot)
		return SegmentIsectClass::point;
	return SegmentIsectClass::none;
}

template <class T, int N> auto Segment<T, N>::closestPoint(const Point &point) const -> PPoint {
	if(empty())
		return {from, T(0)};

	auto vec = to - from;
	auto t = dot(point - from, vec) / fwk::lengthSq(vec);
	t = clamp(t, Scalar(0), Scalar(1));
	return {from + vec * t, t};
}

template <class T, int N> auto Segment<T, N>::closestPoint(const Segment &seg) const -> PPoint {
	return closestPoints(seg).first;
}

// Source: Real-time collision detection (page 150)
template <class T, int N>
auto Segment<T, N>::closestPoints(const Segment &rhs) const -> pair<PPoint, PPoint> {
	if(empty()) {
		auto point2 = rhs.closestPoint(from);
		return {PPoint{from, T(0)}, point2};
	}

	auto r = from - rhs.from;
	auto d1 = to - from, d2 = rhs.to - rhs.from;
	auto a = dot(d1, d1), e = dot(d2, d2);
	auto f = dot(d2, r);

	auto c = dot(d1, r);
	auto b = dot(d1, d2);
	auto denom = a * e - b * b;

	T s = 0, t = 0;

	if(denom != T(0)) {
		s = clamp((b * f - c * e) / denom, T(0), T(1));
	} else {
		// TODO: handle it properly
		s = 0;
	}

	t = (b * s + f) / e;

	if(t < T(0)) {
		t = 0;
		s = clamp(-c / a, T(0), T(1));
	} else if(t > T(1)) {
		t = 1;
		s = clamp((b - c) / a, T(0), T(1));
	}

	return make_pair(PPoint{at(s), s}, PPoint{rhs.at(t), t});
}

// Source: Real-time collision detection (page 130)
template <class T, int N> T Segment<T, N>::distanceSq(const Point &point) const {
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

template <class T, int N> T Segment<T, N>::distanceSq(const Segment &rhs) const {
	auto points = closestPoints(rhs);
	return fwk::distanceSq(points.first.point, points.second.point);
}

// Jak opisać dokładność tej funkcji ?
template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
auto Segment<T, N>::isectParam(const Segment &rhs) const -> IsectParam {
	if(empty()) {
		if(rhs.empty()) {
			if(to == rhs.to)
				return PointParam{T(1)};
			return none;
		}
		return rhs.isectParam(*this);
	}

	auto vec1 = to - from;
	auto vec2 = rhs.to - rhs.from;

	auto tdot = dot(vec1, perpendicular(vec2));
	if(tdot == T(0)) {
		T length_sq = fwk::lengthSq(vec1);

		// lines are parallel
		if(dot(rhs.from - from, perpendicular(vec2)) == T(0)) {
			// lines are the same
			T t1 = dot(rhs.from - from, vec1);
			T t2 = dot(rhs.to - from, vec1);

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, T(0));
			t2 = min(t2, length_sq);

			if(t2 < T(0) || t1 > length_sq)
				return none;
			if(t1 == t2)
				return PointParam{t1 / length_sq};
			return SubSegmentParam{t1 / length_sq, t2 / length_sq};
		}

		return none;
	}

	auto diff = rhs.from - from;
	auto t1 = dot(diff, perpendicular(vec2));
	auto t2 = dot(diff, perpendicular(vec1));
	if(tdot < T(0)) {
		t1 = -t1;
		t2 = -t2;
		tdot = -tdot;
	}

	if(t1 >= T(0) && t1 <= tdot && t2 >= T(0) && t2 <= tdot)
		return PointParam{t1 / tdot};
	return none;
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
auto Segment<T, N>::isect(const Segment &rhs) const -> Isect {
	auto pisect = isectParam(rhs);

	if(const PointParam *pt = pisect)
		return at(pt->param);
	if(const SubSegmentParam *seg = pisect)
		return subSegment(seg->pmin, seg->pmax);
	return none;
}

template struct ISegment<int, 2>;
template struct ISegment<llint, 2>;
template struct ISegment<qint, 2>;

template SegmentIsectClass ISegment<int, 2>::classifyIsect(const ISegment &) const;
template SegmentIsectClass ISegment<llint, 2>::classifyIsect(const ISegment &) const;
template SegmentIsectClass ISegment<qint, 2>::classifyIsect(const ISegment &) const;

template struct Segment<float, 2>;
template struct Segment<float, 3>;
template struct Segment<double, 2>;
template struct Segment<double, 3>;

template auto Segment<float, 2>::isectParam(const Segment &) const -> IsectParam;
template auto Segment<double, 2>::isectParam(const Segment &) const -> IsectParam;

template auto Segment<float, 2>::isect(const Segment &) const -> Isect;
template auto Segment<double, 2>::isect(const Segment &) const -> Isect;

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
