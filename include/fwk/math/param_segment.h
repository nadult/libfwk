#pragma once

#include "fwk/math/line.h"
#include "fwk/math/rational.h"
#include "fwk/math/segment.h"

namespace fwk {

// Parametrized segment represented as line + two parameters
// Line is normalized so that if two lines overlap then their origin & dir
// are the same except dir sign.
// Usually params are rationals with 2x as much bits as base
template <class TBase, class TParam> struct ParamSegment {
	static_assert(!is_rational<TBase> && is_scalar<TBase>);
	static_assert(is_rational<TParam> && is_scalar<TParam>);

	using BVec = vec2<TBase>;
	using PVec = Promote<MakeVec<TParam, 2>>; // TODO: is this promotion needed ?
	using IsectParam = fwk::IsectParam<TParam>;

	static_assert(precise_conversion<Promote<TBase>, TParam>);

	bool sameLine(const ParamSegment &rhs) const {
		return origin == rhs.origin && (dir == rhs.dir || dir == -rhs.dir);
	}

	static Pair<Base<TBase>> normalizeLine(BVec &origin, BVec &dir);
	static bool isNormalized(const BVec &origin, const BVec &dir);

	ParamSegment(const Segment<BVec> &segment) : origin(segment.from), dir(segment.dir()) {
		auto [off, mul] = normalizeLine(origin, dir);
		from_t = TParam(off);
		to_t = TParam(off + mul);
	}

	ParamSegment(const BVec &origin_, const BVec &dir_, TParam param_from, TParam param_to)
		: origin(origin_), dir(dir_), from_t(param_from), to_t(param_to) {
		PASSERT(isNormalized(origin, dir));
		if(from_t > to_t) {
			dir = -dir;
			from_t = -from_t;
			to_t = -to_t;
		}
	}

	ParamSegment(const Line<BVec> &line, TParam param_from, TParam param_to)
		: ParamSegment(line.origin, line.dir, param_from, param_to) {}

	IsectParam isect(const ParamSegment &rhs) const;

	Line<BVec> line() const { return {origin, dir}; }

	PVec at(TParam t) const { return PVec(origin) + PVec(dir) * t; }
	PVec from() const { return at(from_t); }
	PVec to() const { return at(to_t); }

	void operator>>(TextFormatter &out) const;

	BVec origin, dir;
	TParam from_t, to_t;
};

}
