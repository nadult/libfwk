// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

struct OrbitingCamera {
	OrbitingCamera(float3 center = float3(), float distance = 1.0f, float rot_horiz = 0.0f,
				   float rot_vert = 0.0f);

	static Ex<OrbitingCamera> load(CXmlNode);
	void save(XmlNode) const;

	void focus(FBox);

	Segment3<float> makeRay(float2 screen_pos, IRect viewport, Matrix4 view_mat) const;

	Camera toCamera(const CameraParams &) const;
	void move(float2 move, float2 rot, float zoom);

	FWK_ORDER_BY(OrbitingCamera, center, distance, rot_horiz, rot_vert);

	array<float3, 2> forwardRight() const;

	float3 center;
	float distance;
	float rot_horiz, rot_vert;
};

OrbitingCamera lerp(const OrbitingCamera &, const OrbitingCamera &, float t);
}
