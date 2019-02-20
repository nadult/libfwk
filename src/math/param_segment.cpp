#include "fwk/math/param_segment.h"

#include "fwk/format.h"
#include "fwk/math/ext24.h"
#include "fwk/math/gcd.h"

namespace fwk {

#define TEMPLATE template <class TBase, class TParam>
#define PSEG ParamSegment<TBase, TParam>

TEMPLATE auto PSEG::normalizeLine(BVec &origin, BVec &dir) -> Pair<Base<TBase>> {
	if constexpr(is_ext24<TBase>) {
		int sign = dir.x.sign();
		auto mul = gcd(dir.x.gcd(), dir.y.gcd());
		dir = {dir.x.intDivide(mul), dir.y.intDivide(mul)};

		Maybe<Base<TBase>> off;

		int axis = dir.x == 0 ? 1 : 0;

		// Normalizing origin
		for(int n : intRange(4))
			if(dir[axis][n] != 0) {
				auto div = origin[axis][n] / dir[axis][n];
				if(!off || abs(div) < abs(*off))
					off = div;
			}
		PASSERT(off);

		origin -= dir * *off;
		return {*off, mul};

	} else {
		int sign = dir.x < 0 ? -1 : 1;
		auto mul = gcd(dir.x, dir.y);
		dir /= mul;

		auto off = dir.x == 0 ? origin.y / dir.y : origin.x / dir.x;
		origin -= dir * off;

		return {off, mul};
	}
}

TEMPLATE bool PSEG::isNormalized(const BVec &origin, const BVec &dir) {
	auto torigin = origin, tdir = dir;
	normalizeLine(torigin, tdir);
	return torigin == origin && tdir == dir;
}

TEMPLATE auto PSEG::isect(const ParamSegment &rhs) const -> IsectParam {
	using PBVec = Promote<BVec>;
	auto tdot = dot<PBVec>(dir, perpendicular(rhs.dir));
	if(tdot == 0) {
		// lines are parallel
		if(origin == rhs.origin) {
			// same line
			if(dir == rhs.dir) {
				auto tmin = max(from_t, rhs.from_t);
				auto tmax = min(to_t, rhs.to_t);
				return {tmin, tmax};
			} else {
				auto tmin = max(from_t, -rhs.to_t);
				auto tmax = min(to_t, -rhs.from_t);
				return {tmin, tmax};
			}
		}

		return {};
	}

	auto diff = rhs.origin - origin;
	auto t1 = ratDivide(dot<PBVec>(diff, perpendicular(rhs.dir)), tdot);
	auto t2 = ratDivide(dot<PBVec>(diff, perpendicular(dir)), tdot);

	if(t1 >= from_t && t1 <= to_t && t2 >= rhs.from_t && t2 <= rhs.to_t)
		return t1;
	return {};
}

TEMPLATE void PSEG::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "(% : %; % - %)" : "% % % %", origin, dir, from_t, to_t);
}

template struct ParamSegment<short, Rational<int>>;
template struct ParamSegment<int, Rational<llint>>;

template struct ParamSegment<Ext24<short>, Rational<Ext24<int>>>;
template struct ParamSegment<Ext24<int>, Rational<Ext24<llint>>>;

}
