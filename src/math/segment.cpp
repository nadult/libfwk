/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"
#include "fwk_variant.h"
#include "fwk_xml.h"
#include <cmath>
#include <cstdlib>

namespace fwk {

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

	if(dot(vec1, perpendicular(vec2)) == R(0)) {
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

	Vector3<R> affine1[2] = {{seg1.from, R(1)}, {seg1.to, R(1)}};
	Vector3<R> affine2[2] = {{seg2.from, R(1)}, {seg2.to, R(1)}};

	auto aline1 = cross(affine1[0], affine1[1]);
	auto aline2 = cross(affine2[0], affine2[1]);
	auto apoint = cross(aline1, aline2);
	auto point = apoint.xy() / apoint.z;

	R t = dot(point - seg1.from, vec1) / length_sq;
	if(t >= R(0) && t <= R(1))
		return point;
	return none;
}

Segment2Isect<float> intersection(const Segment2<float> &lhs, const Segment2<float> &rhs) {
	return isect(lhs, rhs);
}

Segment2Isect<double> intersection(const Segment2<double> &lhs, const Segment2<double> &rhs) {
	return isect(lhs, rhs);
}
}
