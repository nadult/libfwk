// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

#include "fwk/sys/assert.h"

namespace fwk {

template <class T> double bestIntegralScale(const Box<T> &box, int max_value) {
	auto tvec = vmax(vabs(box.min()), vabs(box.max()));
	auto tmax = max(max(tvec.values()), Scalar<T>(1.0));
	return double(max_value) / tmax;
}

template <class T> Box<T> encloseSelected(CSpan<T> points, CSpan<bool> valids) {
	DASSERT_EQ(points.size(), valids.size());

	Box<T> box;
	int n = 0;
	for(; n < valids.size(); n++)
		if(valids[n]) {
			box = {points[n], points[n]};
			break;
		}
	for(; n < valids.size(); n++)
		if(valids[n])
			box = {vmin(box.min(), points[n]), vmax(box.max(), points[n])};
	return box;
}

#define INSTANTIATE(T)                                                                             \
	template double bestIntegralScale(const Box<T> &, int);                                        \
	template Box<T> encloseSelected(CSpan<T>, CSpan<bool>);
INSTANTIATE(int2)
INSTANTIATE(float2)
INSTANTIATE(double2)
INSTANTIATE(int3)
INSTANTIATE(float3)
INSTANTIATE(double3)
#undef INSTANTIATE

}
