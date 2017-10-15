// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/plane.h"

namespace fwk {

// Basically, a set of planes
class Frustum {
  public:
	enum {
		id_left,
		id_right,
		id_up,
		id_down,
		//	id_front,
		//	id_back

		planes_count,
	};

	Frustum() = default;
	Frustum(const Matrix4 &view_projection);
	Frustum(CSpan<Plane3F, planes_count>);

	bool isIntersecting(const float3 &point) const;
	bool isIntersecting(const FBox &box) const;

	// TODO: clang crashes on this (in full opt with -DNDEBUG)
	bool isIntersecting(CSpan<float3> points) const;

	const Plane3F &operator[](int idx) const { return m_planes[idx]; }
	Plane3F &operator[](int idx) { return m_planes[idx]; }
	int size() const { return planes_count; }

  private:
	array<Plane3F, planes_count> m_planes;
};

const Frustum operator*(const Matrix4 &, const Frustum &);
}
