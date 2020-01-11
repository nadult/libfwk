// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/math/plane.h"

namespace fwk {

DEFINE_ENUM(FrustumPlaneId, left, right, up, down);

// Basically, a set of planes
class Frustum {
  public:
	using PlaneId = FrustumPlaneId;
	static constexpr int plane_count = count<PlaneId>;

	Frustum() = default;
	Frustum(const Matrix4 &view_projection);
	Frustum(CSpan<Plane3F, plane_count>);
	Frustum(EnumMap<PlaneId, Plane3F>);

	bool testIsect(const float3 &point) const;
	bool testIsect(const FBox &box) const;
	bool testIsect(CSpan<float3> points) const;

	const Plane3F &operator[](int idx) const {
		PASSERT(idx >= 0 && idx < plane_count);
		return planes[PlaneId(idx)];
	}
	Plane3F &operator[](int idx) {
		PASSERT(idx >= 0 && idx < plane_count);
		return planes[PlaneId(idx)];
	}

	const Plane3F &operator[](PlaneId id) const { return planes[id]; }
	Plane3F &operator[](PlaneId id) { return planes[id]; }

	EnumMap<PlaneId, Plane3F> planes;
};
}
