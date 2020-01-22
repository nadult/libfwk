// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/camera.h"

#include "fwk/io/xml.h"
#include "fwk/math/frustum.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"

namespace fwk {

CameraParams::CameraParams() = default;

float CameraParams::aspectRatio() const { return float(viewport.width()) / viewport.height(); }

Matrix4 CameraParams::projectionMatrix() const {
	if(projection == CameraProjection::orthogonal)
		return fwk::ortho(viewport.x(), viewport.ex(), viewport.y(), viewport.ey(), depth.min,
						  depth.max);
	return fwk::perspective(fov_in_radians, aspectRatio(), depth.min, depth.max);
}

Camera::Camera(const float3 &pos, const float3 &target, const float3 &target_up_vector,
			   Params params)
	: m_pos(pos), m_target(target), m_target_up(target_up_vector), m_params(params) {
	DASSERT(!isNan(m_pos));
	DASSERT(!isNan(m_target));
	DASSERT(!isNan(m_target_up));
}

Ex<Camera> Camera::load(CXmlNode node) {
	Camera out{node("pos", float3()), node("target", float3(0, 0, 1)),
			   node("target_up", float3(0, 1, 0))};
	EXPECT(out.m_pos != out.m_target);
	out.m_target_up = normalize(out.m_target_up);
	return out;
}

void Camera::save(XmlNode node) const {
	node("pos") = m_pos;
	node("target") = m_target;
	node("target_up") = m_target_up;
}

void Camera::setViewport(IRect viewport) { m_params.viewport = viewport; }

Matrix4 Camera::viewMatrix() const {
	auto ret = fwk::lookAt(m_pos, m_target, up());

	if(m_params.projection == CameraProjection::orthogonal) {
		if(m_params.ortho_depth_offset) {
			float fwd = dot(m_pos, forward());
			float diff = fwd - *m_params.ortho_depth_offset;
			ret = translation(0, 0, -diff) * ret;
		}
		float3 center(float2(m_params.viewport.center()), 0.0f);
		return translation(center) * scaling(m_params.ortho_scale) * ret;
	}

	return ret;
}
Matrix4 Camera::projectionMatrix() const { return m_params.projectionMatrix(); }
Matrix4 Camera::matrix() const { return m_params.projectionMatrix() * viewMatrix(); }
Frustum Camera::frustum() const { return Frustum(matrix()); }

float3 Camera::up() const {
	// TODO: what if dir is close to target_up?
	float3 forward = this->forward();
	float3 right = cross(forward, m_target_up);
	//	printf("right: %.2f %.2f %.2f\n", right.x, right.y, right.z);
	//	printf("forw:  %.2f %.2f %.2f\n", forward.x, forward.y, forward.z);
	return cross(right, forward);
}

float3 Camera::forward() const { return normalize(m_target - m_pos); }

float3 Camera::right() const { return cross(forward(), up()); }

Segment3<float> Camera::screenRay(float2 screen_pos) const {
	float2 pos = screen_pos - float2(m_params.viewport.center());
	pos.x = pos.x * 2.0f / m_params.viewport.width();
	pos.y = -pos.y * 2.0f / m_params.viewport.height();
	return screenRayNormalized(pos);
}

Segment3<float> Camera::screenRayNormalized(float2 screen_pos_nrm) const {
	auto inv_mat = inverseOrZero(matrix());
	auto p1 = mulPoint(inv_mat, float3(screen_pos_nrm, -1.0f));
	auto p2 = mulPoint(inv_mat, float3(screen_pos_nrm, 1.0f));
	return {p1, p2};

	/* // Old strange code
	Matrix4 inv_proj_mat = inverseOrZero(projectionMatrix());
	float3 target_dir = normalize(mulPoint(inv_proj_mat, float3(screen_pos_nrm, 1.0f)));
	auto out =
		inverseOrZero(viewMatrix()) * Segment3<float>(target_dir * 1.0f, target_dir * 1024.0f);
	print("ray: %\n", out);
	return out;*/
}

float2 Camera::screenOffset(const Camera &rhs) const {
	Plane3F plane(normalize(m_target - m_pos), float3());

	auto ray1 = screenRayNormalized(float2());
	auto ray2 = rhs.screenRayNormalized(float2());
	auto ip1 = ray1.isectParamPlane(plane);
	auto ip2 = ray2.isectParamPlane(plane);

	if(!ip1.isPoint() || !ip2.isPoint())
		return {};

	auto pt1 = ray1.at(ip1.asPoint());
	auto pt2 = ray2.at(ip2.asPoint());

	auto screen_pt1 = mulPoint(matrix(), pt1);
	auto screen_pt2 = mulPoint(matrix(), pt2);
	return (screen_pt2 - screen_pt1).xy();
}

FRect Camera::screenRect(FBox box) const {
	FBox trans_box = encloseTransformed(box, matrix());
	FRect out = enclose(transform(
		trans_box.corners(), [](auto pt) { return (float2(pt.x, -pt.y) + float2(1, 1)) * 0.5f; }));
	return out * float2(m_params.viewport.size());
}

Frustum Camera::screenFrustum(FRect rect, Interval<float> depth_interval) const {
	DASSERT(!rect.empty());

	float2 corners[4] = {
		{rect.x(), rect.ey()}, {rect.x(), rect.y()}, {rect.ex(), rect.y()}, {rect.ex(), rect.ey()}};
	float3 points[2][4];
	for(int n = 0; n < 4; n++) {
		auto ray = screenRay(corners[n]);
		points[0][n] = ray.at(depth_interval.min);
		points[1][n] = ray.at(depth_interval.max);
	}

	Frustum out;
	out[FrustumPlaneId::left] = {points[0][0], points[1][0], points[0][1]};
	out[FrustumPlaneId::up] = {points[0][1], points[1][1], points[0][2]};
	out[FrustumPlaneId::right] = {points[0][2], points[1][2], points[0][3]};
	out[FrustumPlaneId::down] = {points[0][3], points[1][3], points[0][0]};
	return out;
}
}
