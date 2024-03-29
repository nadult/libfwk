// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/line.h"

#include "fwk/io/xml.h"
#include "fwk/variant.h"

namespace fwk {

#define TEMPLATE template <c_vec TVec>
#define TLINE Line<TVec>

// TODO: in case of floating-points accuracy can be improved
TEMPLATE auto TLINE::isectParam(const Line &rhs) const -> PRIsectParam {
	if constexpr(dim == 2) {
		auto vec1 = dir, vec2 = rhs.dir;

		PT tdot = dot<PVec>(vec1, perpendicular(vec2));
		if(tdot == PT(0)) {
			// lines are parallel
			if(dot<PVec>(rhs.origin - origin, perpendicular(vec2)) == PT(0)) {

				// lines are the same
				return {-inf, inf};
			}

			return {};
		}

		auto diff = rhs.origin - origin;
		auto t1 = dot<PVec>(diff, perpendicular(vec2));
		return ratDivide(t1, tdot);
	} else {
		FWK_FATAL("write me please");
		return {};
	}
}

TEMPLATE
auto TLINE::closestPointParam(const Point &pt) const -> PRT {
	return ratDivide(dot(dir, pt - origin), dot(dir, dir));
}

TEMPLATE void TLINE::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isStructured() ? "(% : %)" : "% %", origin, dir);
}

#define INSTANTIATE(type)                                                                          \
	template class Line<vec2<type>>;                                                               \
	template class Line<vec3<type>>;

INSTANTIATE(short)
INSTANTIATE(int)
INSTANTIATE(llint)
INSTANTIATE(float)
INSTANTIATE(double)

#undef TEMPLATE

}
