// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom_base.h"

#include "fwk/hash_set.h"
#include "fwk/math/line.h"
#include "fwk/sparse_span.h"
#include "fwk/sys/assert.h"

namespace fwk {

FWK_ORDER_BY_DEF(GLabel, color, ival1, ival2, fval1, fval2);

template <class T> double integralScale(const Box<T> &box, int max_value) {
	auto tvec = vmax(vabs(box.min()), vabs(box.max()));
	auto tmax = max(max(tvec.values()), Scalar<T>(1.0));
	return double(max_value) / tmax;
}

template <c_vec T> Box<T> enclose(SparseSpan<T> points) {
	if(!points)
		return {};
	auto first_pt = points.front();
	Box<T> box{first_pt, first_pt};
	for(auto pt : points)
		box = {vmin(box.min(), pt), vmax(box.max(), pt)};
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

template <c_vec<2> T>
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

Line2<float> approximateLine(CSpan<float2> points) {
	DASSERT(points);

	auto avg = accumulate(points) / float(points.size());
	auto slope = 0.0f;
	auto div = 0.0f;
	for(auto pt : points) {
		slope += (pt.x - avg.x) * (pt.y - avg.y);
		div += (pt.x - avg.x) * (pt.x - avg.x);
	}
	slope /= div;
	float b = avg.y - slope * avg.x;
	float2 dir(1.0f, slope);

	Line line({0, b}, normalize(dir));
	auto center = line.at(line.closestPointParam(avg));
	return {center, line.dir};
}

#define INSTANTIATE3D(T)                                                                           \
	template double integralScale(const Box<T> &, int);                                            \
	template Box<T> enclose(SparseSpan<T>);

#define INSTANTIATE2D(T)                                                                           \
	template double integralScale(const Box<T> &, int);                                            \
	template Box<T> enclose(SparseSpan<T>);                                                        \
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
