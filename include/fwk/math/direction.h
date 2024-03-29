// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"

namespace fwk {

// Returns true if v2 is CCW to v1
template <c_vec<2> T> bool ccwSide(const T &v1, const T &v2) {
	return cross<Promote<T>>(v1, v2) > 0;
}

template <c_vec<2> T> bool cwSide(const T &v1, const T &v2) {
	return cross<Promote<T>>(v1, v2) < 0;
}

// Returns true if (point - from) is CCW to (to - from)
template <c_vec<2> T> bool ccwSide(const T &from, const T &to, const T &point) {
	return dot<Promote<T>>(perpendicular(to - from), point - from) > 0;
}

template <c_vec<2> T> bool cwSide(const T &from, const T &to, const T &point) {
	return dot<Promote<T>>(perpendicular(to - from), point - from) < 0;
}

template <c_vec TVec> bool sameDirection(const TVec &vec1, const TVec &vec2) {
	using PT = PromoteIntegral<TVec>;
	using TCross = If<dim<TVec> == 2, Scalar<PT>, PT>;
	return cross<PT>(vec1, vec2) == TCross(0) && dot<PT>(vec1, vec2) > 0;
}

template <c_vec<2> T> int quadrant(const T &vec) {
	// TODO: add sign function?
	int quad;
	if constexpr(is_rational<T>)
		quad = (vec.numY() < 0 ? 2 : 0) + (vec.numX() < 0 ? 1 : 0);
	else
		quad = (vec[1] < 0 ? 2 : 0) + (vec[0] < 0 ? 1 : 0);
	return quad & 2 ? quad ^ 1 : quad;
}

// TODO: simplify ?
template <c_vec<2> T, class Func, class Result = ApplyResult<Func, int>>
	requires(is_same<Result, T>)
int ccwNext(T vec, int count, const Func &vecs) {
	DASSERT(count > 0);

	int best = -1, negative = -1;
	for(int n = 0; n < count; n++) {
		auto result = cross<Promote<T>>(vec, vecs(n));
		if(result > 0) {
			best = n;
			break;
		}
		if(result == 0 && vec != vecs(n))
			negative = n;
	}

	if(best == -1) { // No CCW vectors present
		if(negative != -1)
			return negative;
		best = 0;
		T best_vec = vecs(0);
		for(int n = 1; n < count; n++) {
			if(cwSide(vecs(best), vecs(n)))
				best = n;
		}
	} else {
		for(int n = best + 1; n < count; n++)
			if(cwSide(vecs(best), vecs(n)) && ccwSide(vec, vecs(n)))
				best = n;
	}

	return best;
}

template <c_vec<2> T> int ccwNext(T vec, CSpan<T> vecs) {
	return ccwNext(vec, vecs.size(), [=](int idx) { return vecs[idx]; });
}

}
