/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

Projection::Projection(const float3 &origin, const float3 &vec_x, const float3 &vec_y)
	: m_base((DASSERT(isNormalized(vec_x)), vec_x), (DASSERT(isNormalized(vec_y)), vec_y),
			 cross(vec_x, vec_y)),
	  m_ibase(transpose(m_base)), m_origin(origin) {}

Projection::Projection(const Triangle &tri)
	: Projection(tri.a(), normalize(tri.edge1()), tri.normal()) {}

float3 Projection::project(const float3 &point) const { return m_ibase * (point - m_origin); }
float3 Projection::unproject(const float3 &point) const { return (m_base * point) + m_origin; }
float3 Projection::projectVector(const float3 &vec) const { return m_ibase * vec; }
float3 Projection::unprojectVector(const float3 &vec) const { return m_base * vec; }

Triangle Projection::project(const Triangle &tri) const {
	return Triangle(project(tri.a()), project(tri.b()), project(tri.c()));
}

Segment Projection::project(const Segment &seg) const {
	return Segment(project(seg.origin()), project(seg.end()));
}
}
