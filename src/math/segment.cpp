// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/segment.h"

#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/plane.h"
#include "fwk/math/ray.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/xml.h"
#include "fwk/variant.h"

namespace fwk {

#define TEMPLATE template <class T, int N>
#define TEMPLATE_REAL TEMPLATE template <class U, EnableIfReal<U>...>
#define TSEG Segment<T, N>

TEMPLATE_REAL Maybe<Ray<T, N>> TSEG::asRay() const {
	if(empty())
		return none;
	return Ray<T, N>(from, normalize(dir()));
}

// Source: RTCD (page 130)
TEMPLATE auto TSEG::distanceSq(const Point &point) const -> PPRT {
	// TODO: verify this
	auto ab = dir(), ac = point - from, bc = point - to;
	PT e = dot<PVec>(ac, ab);
	if(e <= PT(0))
		return dot<PVec>(ac, ac);
	PT f = dot<PVec>(ab, ab);
	if(e >= f)
		return dot<PVec>(bc, bc);

	return PPRT(dot<PVec>(ac, ac)) - divide(PPT(e) * e, f);
}

TEMPLATE auto TSEG::distanceSq(const Segment &rhs) const -> PReal {
	auto points = closestPoints(rhs);
	return fwk::distanceSq(PRealVec(points.first), PRealVec(points.second));
}

TEMPLATE auto TSEG::at(const IsectParam &pisect) const -> Isect {
	if constexpr(is_real<T>) {
		if(pisect.isPoint())
			return at(pisect.asPoint());
		if(pisect.isInterval())
			return subSegment(pisect.asInterval());
	}
	return none;
}

// TODO: in case of floating-points accuracy can be improved
TEMPLATE auto TSEG::isectParam(const Segment &rhs) const -> PRIsectParam {
	for(int n = 0; n < N; n++) {
		auto lmin = min(from[n], to[n]);
		auto lmax = max(from[n], to[n]);
		auto rmin = min(rhs.from[n], rhs.to[n]);
		auto rmax = max(rhs.from[n], rhs.to[n]);

		if(lmin > rmax || rmin > lmax)
			return {};
	}

	if constexpr(N == 2) {
		if(empty()) {
			if(rhs.empty()) {
				if(to == rhs.to)
					return PRT(0);
				return {};
			}

			if(rhs.classifyIsect(from) != IsectClass::none)
				return PRT(0);
			return {};
		}

		auto vec1 = dir(), vec2 = rhs.dir();

		PT tdot = dot<PVec>(vec1, perpendicular(vec2));
		if(tdot == PT(0)) {
			// lines are parallel
			if(dot<PVec>(rhs.from - from, perpendicular(vec2)) == PT(0)) {
				PT length_sq = fwk::lengthSq<PVec>(vec1);

				// lines are the same
				PT t1 = dot<PVec>(rhs.from - from, vec1);
				PT t2 = dot<PVec>(rhs.to - from, vec1);

				if(t2 < t1)
					swap(t1, t2);
				t1 = max(t1, PT(0));
				t2 = min(t2, length_sq);

				if(t2 < PT(0) || t1 > length_sq)
					return {};
				if(t2 == t1)
					return divide(t1, length_sq);

				return {divide(t1, length_sq), divide(t2, length_sq)};
			}

			return {};
		}

		auto diff = rhs.from - from;
		auto t1 = dot<PVec>(diff, perpendicular(vec2));
		auto t2 = dot<PVec>(diff, perpendicular(vec1));
		if(tdot < PT(0)) {
			t1 = -t1;
			t2 = -t2;
			tdot = -tdot;
		}

		if(t1 >= PT(0) && t1 <= tdot && t2 >= PT(0) && t2 <= tdot)
			return divide(t1, tdot);
		return {};
	} else {
		FATAL("write me please");
		return {};
	}
}

// TODO: don't use rays here (we could avoid sqrt)
TEMPLATE auto TSEG::isectParam(const Box<Vec> &box) const -> PRIsectParam {
	if constexpr(is_real<T>) {
		if(empty())
			return box.contains(from) ? T(0) : IsectParam();
		auto param = asRay()->isectParam(box).asInterval() / length();
		return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
	} else {
		FATAL("write me please");
		return {};
	}
}

// TODO: proper intersections (not based on rays)
TEMPLATE_REAL IsectParam<T> TSEG::isectParam(const Plane<T, N> &plane) const {
	if constexpr(N == 2) {
		FATAL("write me");
		return {};
	} else {
		if(empty())
			return plane.signedDistance(from) == T(0) ? T(0) : IsectParam();
		auto param = asRay()->isectParam(plane).asInterval() / length();
		return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
	}
}

TEMPLATE auto TSEG::isectParam(const Triangle<T, N> &tri) const -> pair<PPRIsectParam, bool> {
	if constexpr(N == 3) {
		// TODO: use this in real segment
		Vec ab = tri[1] - tri[0];
		Vec ac = tri[2] - tri[0];

		Vec qp = from - to;
		PVec n = cross<PVec>(ab, ac);

		PPT d = dot<PPVec>(qp, n);
		//	print("n: % % %\n", n[0], n[1], n[2]);

		if(d == 0)
			return {}; // segment parallel: ignore

		Vec ap = from - tri[0];
		PPT t = dot<PPVec>(ap, n);

		bool is_front = d > 0;
		if(d < 0) {
			t = -t;
			d = -d;
		}

		if(t < 0 || t > d)
			return {};

		PVec e = cross<PVec>(qp, ap);
		if(!is_front)
			e = -e;
		PPT v = dot<PPVec>(ac, e);

		if(v < 0 || v > d)
			return {};
		PPT w = -dot<PPVec>(ab, e);

		if(w < 0 || v + w > d)
			return {};

		return {PPRIsectParam(divide<PPT>(t, d)), is_front};
	} else {
		FATAL("write me please");
		return {};
	}
}

TEMPLATE auto TSEG::isect(const Segment &segment) const -> Isect {
	if constexpr(is_real<T>)
		return at(isectParam(segment));

	FATAL("write me");
	return {};
}

TEMPLATE auto TSEG::isect(const Box<Vec> &box) const -> Isect {
	if constexpr(is_real<T>)
		return at(isectParam(box));

	FATAL("write me");
	return {};
}

TEMPLATE IsectClass TSEG::classifyIsect(const Point &point) const {
	if constexpr(N == 2) {
		if(isOneOf(point, from, to))
			return IsectClass::adjacent;
		if(empty())
			return IsectClass::none;

		PVec vec = dir();
		if(cross<PVec>(point - from, vec) == PT(0)) {
			// point lies on the line
			PT length_sq = fwk::lengthSq(vec);
			PT t = dot<PVec>(point - from, vec);

			if(t < PT(0) || t > length_sq)
				return IsectClass::none;
			if(t == PT(0) || t == length_sq)
				return IsectClass::adjacent;
			return IsectClass::point;
		}
	} else {
		FATAL("write me please");
	}

	return IsectClass::none;
}

TEMPLATE IsectClass TSEG::classifyIsect(const Segment &rhs) const {
	// TODO: what if segments overlap with points ?
	PRIsectParam param = isectParam(rhs);
	if(param.isInterval())
		return IsectClass::segment;
	if(param.isPoint()) {
		if(isOneOf(param.closest(), 0, 1))
			return IsectClass::adjacent; // TODO: different kind of adjacency
		return IsectClass::point;
	}
	return IsectClass::none;
}

// Source: RTCD, page: 183
// TODO: make it return IsectClass
TEMPLATE bool TSEG::testIsect(const Box<Vec> &box) const {
	PVec e = box.size();
	PVec d = dir();
	PVec m = PVec(from + to) - PVec(box.max() + box.min());

	PVec ad;
	for(int i = 0; i < N; i++) {
		ad[i] = abs(d[i]);
		if(abs(m[i]) > e[i] + ad[i])
			return false;
	}

	if constexpr(N == 2) {
		return abs(cross(m, d)) <= e[0] * ad[1] + e[1] * ad[0];
	} else {
		// TODO: test it
		if(abs(m.z * d.z - m.z * d.y) > e.y * ad.z + e.z * ad.y)
			return false;
		if(abs(m.z * d.x - m.x * d.z) > e.x * ad.z + e.z * ad.x)
			return false;
		if(abs(m.x * d.y - m.y * d.x) > e.x * ad.y + e.y * ad.x)
			return false;
		return true;
	}
}

TEMPLATE auto TSEG::closestPointParam(const Point &point) const -> PRT {
	if constexpr(is_integral<T>) {
		if(empty())
			return PRT(0);

		PVec vec = dir();
		auto t = PRT(dot<PVec>(point - from, vec), fwk::lengthSq<PVec>(vec));
		if(t.num() < 0)
			return PRT(0);
		if(t.num() > t.den())
			return PRT(1);
		return t;

		if(empty())
			return T(0);
	} else {
		auto vec = dir();
		auto t = dot(point - from, vec) / fwk::lengthSq(vec);
		return clamp(t, Scalar(0), Scalar(1));
	}
}

TEMPLATE auto TSEG::closestPointParam(const Segment &seg) const -> PPRT {
	if(empty())
		return T(0);
	return closestPointParams(seg).first;
}

// Source: Real-time collision detection (page 150)
TEMPLATE auto TSEG::closestPointParams(const Segment &rhs) const -> pair<PPRT, PPRT> {
	if(empty())
		return {PPRT(0), PPRT(rhs.closestPointParam(from))};

	PVec r = from - rhs.from;
	PVec d1 = dir(), d2 = rhs.dir();
	PT a = dot(d1, d1), e = dot(d2, d2);
	PT f = dot(d2, r);

	PT c = dot(d1, r);
	PT b = dot(d1, d2);
	PPT denom = PPT(a) * e - PPT(b) * b;

	PPRT s = 0, t = 0;

	if(denom != PPT(0)) {
		PPT num_s = PPT(b) * f - PPT(c) * e;
		PPT num_t = PPT(a) * f - PPT(b) * c;
		s = clamp01(divide(num_s, denom));
		t = clamp01(divide(num_t, denom));
	} else {
		s = 0;
		t = (PPRT)divide(f, e);
	}

	if(t < PPT(0)) {
		t = 0;
		s = clamp01(divide(PPT(-c), a));
	} else if(t > PPT(1)) {
		t = 1;
		s = clamp01(divide(PPT(b - c), a));
	}

	return {s, t};
}

TEMPLATE auto TSEG::closestPoint(const Point &pt) const -> PRealVec {
	return at(closestPointParam(pt));
}

TEMPLATE auto TSEG::closestPoint(const Segment &seg) const -> PRealVec {
	return at(closestPointParam(seg));
}

TEMPLATE auto TSEG::closestPoints(const Segment &rhs) const -> pair<PRealVec, PRealVec> {
	auto params = closestPointParams(rhs);
	auto params2 = rhs.closestPointParams(*this);
	return {at(params.first), rhs.at(params.second)};
}
TEMPLATE void TSEG::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%; %)" : "% %", from, to);
}

#define INSTANTIATE(type)                                                                          \
	template class Segment<type, 2>;                                                               \
	template class Segment<type, 3>;

INSTANTIATE(short)
INSTANTIATE(int)
INSTANTIATE(llint)
INSTANTIATE(float)
INSTANTIATE(double)

template Maybe<Ray<float, 2>> Segment<float, 2>::asRay() const;
template Maybe<Ray<double, 2>> Segment<double, 2>::asRay() const;

template Maybe<Ray<float, 3>> Segment<float, 3>::asRay() const;
template Maybe<Ray<double, 3>> Segment<double, 3>::asRay() const;

template auto Segment<float, 3>::isectParam(const Plane3<float> &) const -> IsectParam;
template auto Segment<double, 3>::isectParam(const Plane3<double> &) const -> IsectParam;

Segment<float, 3> operator*(const Matrix4 &mat, const Segment<float, 3> &segment) {
	return {mulPoint(mat, segment.from), mulPoint(mat, segment.to)};
}
}
