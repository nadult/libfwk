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

#ifdef __clang__
template <class Seg> static IsectClass classifyIsect(const Seg &, const Seg &) ALWAYS_INLINE;
template <class Seg, class Box> static bool testIsect(const Seg &, const Box &) ALWAYS_INLINE;
#endif

template <class Seg, class Point>
static IsectClass classifyIsect(const Seg &seg, const Point &point) {
	using T = typename Seg::Scalar;
	using IClass = IsectClass;

	if(isOneOf(point, seg.from, seg.to))
		return IClass::adjacent;
	if(seg.empty())
		return IClass::none;

	auto vec = seg.to - seg.from;
	if(cross(point - seg.from, vec) == T(0)) {
		// point lies on the line
		T length_sq = lengthSq(vec);
		T t = dot(point - seg.from, vec);

		if(t < T(0) || t > length_sq)
			return IClass::none;
		if(t == T(0) || t == length_sq)
			return IClass::adjacent;
		return IClass::point;
	}

	return IClass::none;
}

template <class Seg> static IsectClass classifyIsect(const Seg &lhs, const Seg &rhs) {
	using T = typename Seg::Scalar;
	using IClass = IsectClass;

	if(lhs.empty()) {
		if(rhs.empty())
			return lhs.from == rhs.from ? IClass::adjacent : IClass::none;
		return classifyIsect(rhs, lhs.from);
	}
	if(rhs.empty())
		return classifyIsect(lhs, rhs.from);

	auto vec1 = lhs.to - lhs.from;
	auto vec2 = rhs.to - rhs.from;

	auto tdot = dot(vec1, perpendicular(vec2));
	if(tdot == T(0)) {
		// lines are parallel
		if(dot(rhs.from - lhs.from, perpendicular(vec2)) == T(0)) {
			// lines are the same
			T length_sq = lengthSq(vec1);
			T t1 = dot(rhs.from - lhs.from, vec1);
			T t2 = dot(rhs.to - lhs.from, vec1);

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, T(0));
			t2 = min(t2, T(length_sq));

			if(t2 < T(0) || t1 > length_sq)
				return IClass::none;
			if(isOneOf(t1, T(0), length_sq) && isOneOf(t2, T(0), length_sq))
				return IClass::adjacent;
			if(t1 == t2)
				return IClass::point;
			return IClass::segment;
		}

		return IClass::none;
	}

	if(lhs.adjacent(rhs))
		return IClass::adjacent;

	auto diff = rhs.from - lhs.from;
	auto t1 = dot(diff, perpendicular(vec2));
	auto t2 = dot(diff, perpendicular(vec1));

	if(tdot < T(0)) {
		t1 = -t1;
		t2 = -t2;
		tdot = -tdot;
	}

	if(t1 >= T(0) && t1 <= tdot && t2 >= T(0) && t2 <= tdot)
		return IClass::point;
	return IClass::none;
}

// Source: RTCD, page: 183
// TODO: make it return IsectClass
template <class Seg, class Box> bool testIsect(const Seg &seg, const Box &box) {
	using Vec = typename Seg::Vector;
	enum { dim_size = Seg::dim_size };

	auto e = box.size();
	auto d = seg.to - seg.from;
	auto m = (seg.from + seg.to) - (box.max() + box.min());

	Vec ad;
	for(int i = 0; i < dim_size; i++) {
		ad[i] = abs(d[i]);
		if(abs(m[i]) > e[i] + ad[i])
			return false;
	}

	if constexpr(dim_size == 2) {
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

template <class T, class PT, class Ret, class Seg> Ret isect2D(const Seg &lhs, const Seg &rhs) {
	using Vec = MakeVec<T, 2>;
	using PVec = MakeVec<PT, 2>;
	using RT = Conditional<isIntegral<T>(), Rational<PT>, PT>;

	if(lhs.empty()) {
		if(rhs.empty()) {
			if(lhs.to == rhs.to)
				return RT(0);
			return {};
		}

		if(rhs.classifyIsect(lhs.from) != IsectClass::none)
			return RT(0);
		return {};
	}

	auto vec1 = lhs.to - lhs.from;
	auto vec2 = rhs.to - rhs.from;

	auto divide = [](PT num, PT den) {
		if constexpr(isIntegral<T>())
			return RT(num, den);
		else
			return num / den;
	};

	PT tdot = dot<PVec>(vec1, perpendicular(vec2));
	if(tdot == PT(0)) {
		// lines are parallel
		if(dot<PVec>(rhs.from - lhs.from, perpendicular(vec2)) == PT(0)) {
			PT length_sq = fwk::lengthSq<PVec>(vec1);

			// lines are the same
			PT t1 = dot<PVec>(rhs.from - lhs.from, vec1);
			PT t2 = dot<PVec>(rhs.to - lhs.from, vec1);

			if(t2 < t1)
				swap(t1, t2);
			t1 = max(t1, PT(0));
			t2 = min(t2, length_sq);

			if(t2 < PT(0) || t1 > length_sq)
				return {};
			if(t2 == t1)
				return RT(t1);

			return {divide(t1, length_sq), divide(t2, length_sq)};
		}

		return {};
	}

	auto diff = rhs.from - lhs.from;
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
}
template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
IsectClass ISegment<T, N>::classifyIsect(const ISegment<T, N> &rhs) const {
	return fwk::classifyIsect(ISegment<qint, 2>(*this), ISegment<qint, 2>(rhs));
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
IsectClass Segment<T, N>::classifyIsect(const Segment<T, N> &rhs) const {
	return fwk::classifyIsect(*this, rhs);
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
IsectClass ISegment<T, N>::classifyIsect(const Point &point) const {
	return fwk::classifyIsect(ISegment<qint, 2>(*this), qint2(point));
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
IsectClass Segment<T, N>::classifyIsect(const Point &point) const {
	return fwk::classifyIsect(*this, point);
}
template <class T, int N> bool ISegment<T, N>::testIsect(const Box<Vector> &box) const {
	using QVec = MakeVec<qint, N>;
	return fwk::testIsect(ISegment<qint, N>(*this), Box<QVec>(box));
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
auto ISegment<T, N>::isectParam(const Triangle3<T> &tri) const -> pair<PPIsectParam, bool> {
	// TODO: use this in real segment
	int3 ab = int3(tri[1]) - int3(tri[0]);
	int3 ac = int3(tri[2]) - int3(tri[0]);

	int3 qp = int3(from) - int3(to);
	llint3 n = cross(llint3(ab), llint3(ac));

	qint d = dot<qint3>(qint3(qp), qint3(n));
	//	print("n: % % %\n", n[0], n[1], n[2]);

	if(d == 0)
		return {}; // segment parallel: ignore

	int3 ap = int3(from) - int3(tri[0]);
	qint t = dot(qint3(ap), qint3(n));

	bool is_front = d > 0;
	if(d < 0) {
		t = -t;
		d = -d;
	}

	if(t < 0 || t > d)
		return {};

	llint3 e = cross(llint3(qp), llint3(ap));
	if(!is_front)
		e = -e;
	qint v = dot(qint3(ac), qint3(e));

	if(v < 0 || v > d)
		return {};
	qint w = -dot(qint3(ab), qint3(e));

	if(w < 0 || v + w > d)
		return {};

	Rational<PPScalar> out(t, d);
	return {out, is_front};
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
auto ISegment<T, N>::isectParam(const ISegment &rhs) const -> PIsectParam {
	return isect2D<T, PScalar, PIsectParam>(*this, rhs);
}

template <class T, int N>
auto ISegment<T, N>::closestPointParam(const Point &point) const -> PRScalar {
	if(empty())
		return PRScalar(0);

	using PVec = Promote<Vector>;
	auto vec = to - from;
	auto t = PRScalar(dot<PVec>(point - from, vec), fwk::lengthSq<PVec>(vec));
	if(t.num() < 0)
		return PRScalar(0);
	if(t.num() > t.den())
		return PRScalar(1);
	return t;
}

template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
bool Segment<T, N>::testIsect(const Box<Vector> &box) const {
	return fwk::testIsect(*this, box);
}

template <class T, int N> Maybe<Ray<T, N>> Segment<T, N>::asRay() const {
	if(empty())
		return none;
	return Ray<T, N>(from, normalize(to - from));
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

template <class T, int N> auto Segment<T, N>::at(const IsectParam &pisect) const -> Isect {
	if(pisect.isPoint())
		return at(pisect.asPoint());
	if(pisect.isInterval())
		return subSegment(pisect.asInterval());
	return none;
}

// Jak opisać dokładność tej funkcji ?
template <class T, int N>
template <class U, EnableInDimension<U, 2>...>
IsectParam<T> Segment<T, N>::isectParam(const Segment &rhs) const {
	return isect2D<T, T, IsectParam>(*this, rhs);
}

// TODO: don't use rays here (we could avoid sqrt)
template <class T, int N> IsectParam<T> Segment<T, N>::isectParam(const Box<Vector> &box) const {
	if(empty())
		return box.contains(from) ? T(0) : IsectParam();

	auto param = asRay()->isectParam(box).asInterval() / length();
	return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
}

// TODO: proper intersections (not based on rays)
template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
IsectParam<T> Segment<T, N>::isectParam(const Plane<T, N> &plane) const {
	if(empty())
		return plane.signedDistance(from) == T(0) ? T(0) : IsectParam();

	auto param = asRay()->isectParam(plane).asInterval() / length();
	return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
IsectParam<T> Segment<T, N>::isectParam(const Triangle<T, N> &tri) const {
	if(empty())
		return tri.distance(from) == T(0) ? T(0) : IsectParam();

	auto param = asRay()->isectParam(tri).asInterval() / length();
	return param.min > T(1) || param.max < T(0) ? IsectParam() : param;
}

template <class T, int N> void ISegment<T, N>::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%; %)" : "% %", from, to);
}

template <class T, int N> void Segment<T, N>::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(%; %)" : "% %", from, to);
}

template class ISegment<short, 2>;
template class ISegment<int, 2>;
template class ISegment<llint, 2>;
template class ISegment<qint, 2>;

template class ISegment<short, 3>;
template class ISegment<int, 3>;
template class ISegment<llint, 3>;
template class ISegment<qint, 3>;

template IsectClass ISegment<short, 2>::classifyIsect(const ISegment &) const;
template IsectClass ISegment<int, 2>::classifyIsect(const ISegment &) const;
template IsectClass ISegment<llint, 2>::classifyIsect(const ISegment &) const;
template IsectClass ISegment<qint, 2>::classifyIsect(const ISegment &) const;

template IsectClass ISegment<short, 2>::classifyIsect(const Point &) const;
template IsectClass ISegment<int, 2>::classifyIsect(const Point &) const;
template IsectClass ISegment<llint, 2>::classifyIsect(const Point &) const;
template IsectClass ISegment<qint, 2>::classifyIsect(const Point &) const;

template pair<IsectParam<Rational<qint>>, bool>
ISegment<int, 3>::isectParam(const Triangle3<int> &) const;

template IsectParam<Rational<llint>> ISegment<int, 2>::isectParam(const ISegment &) const;

template class Segment<float, 2>;
template class Segment<float, 3>;
template class Segment<double, 2>;
template class Segment<double, 3>;

template auto Segment<float, 2>::isectParam(const Segment &) const -> IsectParam;
template auto Segment<double, 2>::isectParam(const Segment &) const -> IsectParam;

template auto Segment<float, 3>::isectParam(const Triangle3<float> &) const -> IsectParam;
template auto Segment<double, 3>::isectParam(const Triangle3<double> &) const -> IsectParam;

template auto Segment<float, 3>::isectParam(const Plane3<float> &) const -> IsectParam;
template auto Segment<double, 3>::isectParam(const Plane3<double> &) const -> IsectParam;

template auto Segment<float, 2>::isect(const Segment &) const -> Isect;
template auto Segment<double, 2>::isect(const Segment &) const -> Isect;

template IsectClass Segment<float, 2>::classifyIsect(const Segment &) const;
template IsectClass Segment<double, 2>::classifyIsect(const Segment &) const;

template IsectClass Segment<float, 2>::classifyIsect(const Point &) const;
template IsectClass Segment<double, 2>::classifyIsect(const Point &) const;

template bool Segment<float, 2>::testIsect(const Box<Vector> &) const;
template bool Segment<double, 2>::testIsect(const Box<Vector> &) const;

Segment<float, 3> operator*(const Matrix4 &mat, const Segment<float, 3> &segment) {
	return {mulPoint(mat, segment.from), mulPoint(mat, segment.to)};
}
}
