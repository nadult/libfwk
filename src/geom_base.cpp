// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

#include "fwk/hash_set.h"
#include "fwk/sys/assert.h"

namespace fwk {

template <class T> double integralScale(const Box<T> &box, int max_value) {
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

template <class T> Ex<vector<int2>> toIntegral(CSpan<T> points, double scale) {
	auto ipoints = transform(points, [=](auto pt) { return int2(pt * scale); });
	HashSet<int2> set;
	for(auto ip : ipoints) {
		if(set.contains(ip))
			return ERROR("Duplicate points found!");
		set.emplace(ip);
	}
	return ipoints;
}

template Ex<vector<int2>> toIntegral(CSpan<float2>, double);
template Ex<vector<int2>> toIntegral(CSpan<double2>, double);

template <class T, EnableIfVec<T, 2>...>
void orderByDirection(Span<int> indices, CSpan<T> vectors, const T &zero_vector) {
	using PT = PromoteIntegral<T>;
	auto it = std::partition(begin(indices), end(indices), [=](int id) {
		auto tcross = cross<PT>(vectors[id], zero_vector);
		if(tcross == 0) {
			return dot<PT>(vectors[id], zero_vector) > 0;
		}
		return tcross < 0;
	});

	auto func1 = [=](int a, int b) { return cross<PT>(vectors[a], vectors[b]) > 0; };
	auto func2 = [=](int a, int b) { return cross<PT>(vectors[a], vectors[b]) > 0; };

	std::sort(begin(indices), it, func1);
	std::sort(it, end(indices), func2);
}

#define INSTANTIATE3D(T)                                                                           \
	template double integralScale(const Box<T> &, int);                                            \
	template Box<T> encloseSelected(CSpan<T>, CSpan<bool>);

#define INSTANTIATE2D(T)                                                                           \
	INSTANTIATE3D(T)                                                                               \
	template void orderByDirection(Span<int>, CSpan<T>, const T &);

INSTANTIATE2D(short2) // TODO: it shouldnt be required
INSTANTIATE2D(int2)
INSTANTIATE2D(float2)
INSTANTIATE2D(double2)
INSTANTIATE3D(int3)
INSTANTIATE3D(float3)
INSTANTIATE3D(double3)

#undef INSTANTIATE

}
