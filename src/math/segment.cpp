// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/segment.h"

#include "fwk/io/xml.h"
#include "fwk/math/box.h"
#include "fwk/math/plane.h"
#include "fwk/math/ray.h"
#include "fwk/math/triangle.h"
#include "fwk/variant.h"

namespace fwk {

#define TEMPLATE template <class TVec>
#define TEMPLATE_REAL TEMPLATE template <class U, EnableIfFpt<U>...>
#define TSEG Segment<TVec>

TEMPLATE_REAL auto TSEG::asRay() const -> Maybe<Ray<T, dim>> {
	if(empty())
		return none;
	return Ray<T, dim>(from, normalize(dir()));
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

	return PPRT(dot<PVec>(ac, ac)) - ratDivide(PPT(e) * e, f);
}

TEMPLATE auto TSEG::distanceSq(const Segment &rhs) const -> PReal {
	auto points = closestPoints(rhs);
	return fwk::distanceSq(PRealVec(points.first), PRealVec(points.second));
}

TEMPLATE auto TSEG::at(const IsectParam &pisect) const -> Isect {
	if constexpr(is_fpt<T>) {
		if(pisect.isPoint())
			return at(pisect.asPoint());
		if(pisect.isInterval())
			return subSegment(pisect.asInterval());
	}
	return none;
}

// TODO: in case of floating-points accuracy can be improved
TEMPLATE auto TSEG::isectParam(const Segment &rhs) const -> PRIsectParam {
	for(int n = 0; n < dim; n++) {
		auto lmin = min(from[n], to[n]);
		auto lmax = max(from[n], to[n]);
		auto rmin = min(rhs.from[n], rhs.to[n]);
		auto rmax = max(rhs.from[n], rhs.to[n]);

		if(lmin > rmax || rmin > lmax)
			return {};
	}

	if constexpr(dim == 2) {
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
					return ratDivide(t1, length_sq);

				return {ratDivide(t1, length_sq), ratDivide(t2, length_sq)};
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
			return ratDivide(t1, tdot);
		return {};
	} else {
		FATAL("write me please");
		return {};
	}
}

// TODO: don't use rays here (we could avoid sqrt)
TEMPLATE auto TSEG::isectParam(const Box<Vec> &box) const -> PRIsectParam {
	if constexpr(is_fpt<T>) {
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
TEMPLATE_REAL auto TSEG::isectParamPlane(const Plane<T, dim> &plane) const -> IsectParam {
	if constexpr(dim == 2) {
		FATAL("write me");
		return {};
	} else {
		if(empty())
			return plane.signedDistance(from) == T(0) ? T(0) : IsectParam();
		auto param = asRay()->isectParam(plane).asInterval() / length();
		return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
	}
}

TEMPLATE auto TSEG::isectParam(const Triangle<T, dim> &tri) const -> Pair<PPRIsectParam, bool> {
	if constexpr(dim == 3) {
		// TODO: it's not robust in reals (especially with broken data)
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

		bool is_front = d > PPT(0);
		if(d < 0) {
			t = -t;
			d = -d;
		}

		if(!(t >= 0 && t <= d))
			return {};

		PVec e = cross<PVec>(qp, ap);
		if(!is_front)
			e = -e;
		PPT v = dot<PPVec>(ac, e);

		if(v < 0 || v > d)
			return {};
		PPT w = -dot<PPVec>(ab, e);

		// Ordering matters for Nans
		if(w >= 0 && v + w <= d)
			return {PPRIsectParam(ratDivide<PPT>(t, d)), is_front};
		return {};
	} else {
		FATAL("write me please");
		return {};
	}
}

TEMPLATE auto TSEG::isect(const Segment &segment) const -> Isect {
	if constexpr(is_fpt<T>)
		return at(isectParam(segment));

	FATAL("write me");
	return {};
}

TEMPLATE auto TSEG::isect(const Box<Vec> &box) const -> Isect {
	if constexpr(is_fpt<T>)
		return at(isectParam(box));

	FATAL("write me");
	return {};
}

TEMPLATE IsectClass TSEG::classifyIsect(const Point &point) const {
	if constexpr(dim == 2) {
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
	for(int i = 0; i < dim; i++) {
		ad[i] = abs(d[i]);
		if(abs(m[i]) > e[i] + ad[i])
			return false;
	}

	if constexpr(dim == 2) {
		return abs(cross(m, d)) <= e[0] * ad[1] + e[1] * ad[0];
	} else {
		// TODO: test it
		if(abs(m[1] * d[2] - m[2] * d[1]) > e[1] * ad[2] + e[2] * ad[1])
			return false;
		if(abs(m[2] * d[0] - m[0] * d[2]) > e[0] * ad[2] + e[2] * ad[0])
			return false;
		if(abs(m[0] * d[1] - m[1] * d[0]) > e[0] * ad[1] + e[1] * ad[0])
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
			return PRT(0);
	} else {
		auto vec = dir();
		auto t = ratDivide(dot(point - from, vec), fwk::lengthSq(vec));
		return clamp01(t);
	}
}

TEMPLATE auto TSEG::closestPointParam(const Segment &seg) const -> PPRT {
	if(empty())
		return T(0);
	return closestPointParams(seg).first;
}

// Source: Real-time collision detection (page 150)
TEMPLATE auto TSEG::closestPointParams(const Segment &rhs) const -> Pair<PPRT> {
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

	if(denom != PPRT(0)) {
		PPT num_s = PPT(b) * f - PPT(c) * e;
		PPT num_t = PPT(a) * f - PPT(b) * c;
		s = clamp01(ratDivide(num_s, denom));
		t = clamp01(ratDivide(num_t, denom));
	} else {
		s = 0;
		t = (PPRT)ratDivide(f, e);
	}

	if(t < PPT(0)) {
		t = 0;
		s = clamp01(ratDivide(PPT(-c), a));
	} else if(t > PPRT(1)) {
		t = 1;
		s = clamp01(ratDivide(PPT(b - c), a));
	}

	return {s, t};
}

TEMPLATE void TSEG::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%; %)" : "% %", from, to);
}

TEMPLATE auto TSEG::clip(const Box<TVec> &input_rect) const -> Maybe<Segment<PRealVec>> {
	// Twin edges had to be treated identically
	if(to < from) {
		auto clipped = Segment{to, from}.clip(input_rect);
		if(clipped)
			return clipped->twin();
		return none;
	}

	if constexpr(dim == 3) {
		FATAL("write me");
	} else {
		// TODO: naming
		// TODO: early out

		using Vec2 = PRealVec;
		using Real = PReal;

		Box<Vec2> rect(input_rect);

		auto offset = Vec2(to - from);
		auto inv_dir = vinv(offset);
		auto origin = Vec2(from);

		Real x1 = inv_dir.x * (rect.x() - origin.x);
		Real x2 = inv_dir.x * (rect.ex() - origin.x);
		Real xmin = min(x1, x2);
		Real xmax = max(x1, x2);

		Real y1 = inv_dir.y * (rect.y() - origin.y);
		Real y2 = inv_dir.y * (rect.ey() - origin.y);
		Real ymin = min(y1, y2);
		Real ymax = max(y1, y2);

		Real lmin = max(xmin, ymin);
		Real lmax = min(xmax, ymax);

		if(lmin >= lmax || lmin >= 1.0 || lmax <= 0.0)
			return none;

		Vec2 out_from(from), out_to(to);
		if(lmin > 0.0 && !rect.contains((Vec2)from)) {
			Real clipx = lmin == x1 ? rect.x() : rect.ex();
			Real clipy = lmin == y1 ? rect.y() : rect.ey();
			out_from = lmin == xmin ? Vec2(clipx, max(origin.y + offset.y * lmin, rect.y()))
									: Vec2(max(origin.x + offset.x * lmin, rect.x()), clipy);
		}

		if(lmax < 1.0 && !rect.contains((Vec2)to)) {
			Real clipx = lmax == x1 ? rect.x() : rect.ex();
			Real clipy = lmax == y1 ? rect.y() : rect.ey();
			out_to = lmax == xmax ? Vec2(clipx, min(origin.y + offset.y * lmax, rect.ey()))
								  : Vec2(min(origin.x + offset.x * lmax, rect.ex()), clipy);
		}

		if(out_from == out_to)
			return none; // TODO: return vec2 here ?
		return Segment<PRealVec>{out_from, out_to};
	}
}

#define INSTANTIATE_VEC(type)                                                                      \
	template class Segment<vec2<type>>;                                                            \
	template class Segment<vec3<type>>;

INSTANTIATE_VEC(short)
INSTANTIATE_VEC(int)
INSTANTIATE_VEC(llint)
INSTANTIATE_VEC(float)
INSTANTIATE_VEC(double)

template Maybe<Ray<float, 2>> Segment<float2>::asRay() const;
template Maybe<Ray<double, 2>> Segment<double2>::asRay() const;

template Maybe<Ray<float, 3>> Segment<float3>::asRay() const;
template Maybe<Ray<double, 3>> Segment<double3>::asRay() const;

template auto Segment<float3>::isectParamPlane(const Plane3<float> &) const -> IsectParam;
template auto Segment<double3>::isectParamPlane(const Plane3<double> &) const -> IsectParam;
}
