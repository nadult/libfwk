// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom_base.h"

#include "fwk/hash_set.h"
#include "fwk/sparse_span.h"
#include "fwk/sys/assert.h"

namespace fwk {

FWK_ORDER_BY_DEF(GLabel, color, ival1, ival2, fval1, fval2);

template <class T> double integralScale(const Box<T> &box, int max_value) {
	auto tvec = vmax(vabs(box.min()), vabs(box.max()));
	auto tmax = max(max(tvec.values()), Scalar<T>(1.0));
	return double(max_value) / tmax;
}

template <class T, EnableIfVec<T>...> Box<T> enclose(SparseSpan<T> points) {
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

template <class T, class T2, EnableIfVec<T, 3>...>
PodVector<T2> planarProjection(SparseSpan<T> points, Axes2D axes) {
	PodVector<T2> out(points.endIndex());
	switch(axes) {
	case Axes2D::xy:
		for(int n : points.indices())
			out[n] = points[n].xy();
		break;
	case Axes2D::xz:
		for(int n : points.indices())
			out[n] = points[n].xz();
		break;
	case Axes2D::yz:
		for(int n : points.indices())
			out[n] = points[n].yz();
		break;
	}
	return out;
}

#define INSTANTIATE3D(T)                                                                           \
	template double integralScale(const Box<T> &, int);                                            \
	template Box<T> enclose(SparseSpan<T>);                                                        \
	template PodVector<MakeVec<Scalar<T>, 2>> planarProjection(SparseSpan<T>, Axes2D);

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
