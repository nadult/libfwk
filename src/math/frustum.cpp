/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_math.h"

namespace fwk {

static const Plane makePlane(const float4 &vec) { return normalize(Plane(vec.xyz(), -vec.w)); }

Frustum::Frustum(const Matrix4 &view_proj) {
	Matrix4 t = transpose(view_proj);

	m_planes[id_left] = makePlane(t[3] + t[0]);
	m_planes[id_right] = makePlane(t[3] - t[0]);

	m_planes[id_up] = makePlane(t[3] - t[1]);
	m_planes[id_down] = makePlane(t[3] + t[1]);

	//	m_planes[id_front]	= makePlane(t[3] - t[2]);
	//	m_planes[id_back]	= makePlane(t[3] + t[2]);

	for(int n = 0; n < planes_count; n++)
		m_planes[n] = m_planes[n];
}

Frustum::Frustum(TRange<Plane, planes_count> planes) {
	for(int n = 0; n < planes_count; n++)
		m_planes[n] = planes[n];
}

bool Frustum::isIntersecting(const float3 &point) const {
	for(int n = 0; n < planes_count; n++)
		if(dot(m_planes[n], point) <= 0)
			return false;
	return true;
}

bool Frustum::isIntersecting(CRange<float3> points) const {
	for(const auto &plane : m_planes) {
		bool all_outside = true;
		for(const auto &point : points)
			if(dot(plane, point) > 0.0f) {
				all_outside = false;
				break;
			}

		if(all_outside)
			return false;
	}
	return true;
}

bool Frustum::isIntersecting(const FBox &box) const {
	float3 points[8];
	box.getCorners(points);
	return isIntersecting(points);
}

const Frustum operator*(const Matrix4 &matrix, const Frustum &frustum) {
	Frustum out;
	for(size_t n = 0; n < Frustum::planes_count; n++)
		out[n] = matrix * frustum[n];
	return out;
}
}
