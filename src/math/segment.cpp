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

template <class T, int N> Maybe<TRay<T, N>> Segment<T, N>::asRay() const {
	if(empty())
		return none;
	return TRay<T, N>(from, normalize(to - from));
}

template <class T, int N> T Segment<T, N>::closestPointParam(const Point &point) const {
	if(empty())
		return T(0);

	auto vec = to - from;
	auto t = dot(point - from, vec) / fwk::lengthSq(vec);
	return clamp(t, Scalar(0), Scalar(1));
}

template <class T, int N> T Segment<T, N>::closestPointParam(const Segment &seg) const {
	if(empty())
		return T(0);
	return closestPointParams(seg).first;
}

// Source: Real-time collision detection (page 150)
template <class T, int N> pair<T, T> Segment<T, N>::closestPointParams(const Segment &rhs) const {
	if(empty())
		return {T(0), rhs.closestPointParam(from)};

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

	return {s, t};
}

template <class T, int N> auto Segment<T, N>::closestPoint(const Point &pt) const -> Vector {
	return at(closestPointParam(pt));
}

template <class T, int N> auto Segment<T, N>::closestPoint(const Segment &seg) const -> Vector {
	return at(closestPointParam(seg));
}

template <class T, int N>
auto Segment<T, N>::closestPoints(const Segment &rhs) const -> pair<Vector, Vector> {
	auto params = closestPointParams(rhs);
	return {at(params.first), rhs.at(params.second)};
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
	return fwk::distanceSq(points.first, points.second);
}

// Jak opisać dokładność tej funkcji ?
template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
auto Segment<T, N>::isectParam(const Segment &rhs) const -> IsectParam {
	if(empty()) {
		if(rhs.empty()) {
			if(to == rhs.to)
				return T(0);
			return none;
		}

		if(rhs.closestPoint(from) == from)
			return T(0);
		return none;
	}

	auto vec1 = to - from;
	auto vec2 = rhs.to - rhs.from;

	auto tdot = dot(vec1, perpendicular(vec2));
	if(tdot == T(0)) {
		// lines are parallel
		if(dot(rhs.from - from, perpendicular(vec2)) == T(0)) {
			T length_sq = fwk::lengthSq(vec1);

			// lines are the same
			T t1 = dot(rhs.from - from, vec1);
			T t2 = dot(rhs.to - from, vec1);

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, T(0));
			t2 = min(t2, length_sq);
			auto ilen = T(1) / length_sq;

			if(t2 < T(0) || t1 > length_sq)
				return none;
			if(t2 == t1)
				return t1;

			return make_pair(t1 * ilen, t2 * ilen);
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
		return t1 / tdot;
	return none;
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
auto Segment<T, N>::isect(const Segment &rhs) const -> Isect {
	auto pisect = isectParam(rhs);

	if(const T *pt = pisect)
		return at(*pt);
	if(const pair<T, T> *seg = pisect)
		return subSegment(seg->first, seg->second);
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
