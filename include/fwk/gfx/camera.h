// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include "fwk/math/interval.h"
#include "fwk/math/rotation.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(CameraProjection, orthogonal, perspective);
DEFINE_ENUM(CameraType, orbiting, fpp, plane, ortho);

struct CameraParams {
	CameraParams();

	FWK_ORDER_BY(CameraParams, viewport, fov_in_radians, ortho_scale, depth, projection);

	Matrix4 projectionMatrix() const;
	float aspectRatio() const;

	IRect viewport = IRect(0, 0, 1280, 720);
	float fov_in_radians = degToRad(60.0f);
	float ortho_scale = 1.0f;
	Interval<float> depth = {0.125f, 32.0f};
	Maybe<float> ortho_depth_offset;
	CameraProjection projection = CameraProjection::perspective;
};

// Generic camera
// Keeps all important information required to generate view & projection matrices
//
// TODO: not very useful for orthogonal cameras ; Separate class for OrthoCamera ?
// TODO: fix ortho camera depth_min, depth_max (do this properly)
class Camera {
  public:
	using Params = CameraParams;

	Camera(const float3 &pos = float3(0, 0, 0), const float3 &target = float3(1, 0, 0),
		   const float3 &target_up_vector = float3(0, 1, 0), Params = Params());

	static Ex<Camera> load(CXmlNode);
	void save(XmlNode) const;

	void setViewport(IRect);

	Matrix4 viewMatrix() const;
	Matrix4 projectionMatrix() const;
	Matrix4 matrix() const;
	Frustum frustum() const;

	const float3 &pos() const { return m_pos; }
	const float3 &targetUp() const { return m_target_up; }
	const float3 &target() const { return m_target; }

	const Params &params() const { return m_params; }
	float targetDistance() const { return distance(m_pos, m_target); }

	float3 up() const;
	float3 forward() const;
	float3 right() const;

	float2 screenOffset(const Camera &) const;
	Segment3<float> screenRay(float2 screen_pos) const;
	Segment3<float> screenRayNormalized(float2 screen_pos_normalized) const;
	FRect screenRect(FBox) const;
	Frustum screenFrustum(FRect rect, Interval<float> depth_interval = {1, 10.0f}) const;

	FWK_ORDER_BY(Camera, m_pos, m_target, m_target_up, m_params);

	// TODO: to jest chujowe
  private:
	float3 m_pos, m_target;
	float3 m_target_up;
	Params m_params;
};
}
