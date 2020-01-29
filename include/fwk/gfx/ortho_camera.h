// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

class Camera;
struct CameraParams;

struct OrthoCamera {
	OrthoCamera(float3 focus = {}, float2 forward_xz = float2(1, 0), float rot_vert = 0.0f,
				float2 xy_offset = {}, float zoom = 20.0f);
	FWK_COPYABLE_CLASS(OrthoCamera);

	static Ex<OrthoCamera> load(CXmlNode);
	void save(XmlNode) const;

	static OrthoCamera closest(const Camera &);
	Camera toCamera(const CameraParams &) const;

	OrthoCamera offsetCamera(float2 offset) const;
	float3 center() const;
	float2 xyOffset(float3 point) const;
	void applyXYOffset();

	void move(float2 move, float2 rot, float move_up);
	void focus(FBox);

	pair<float3, float3> forwardRight() const;

	FWK_ORDER_BY(OrthoCamera, pos, forward_xz, rot_vert, xy_offset, zoom, rot_vert_limit);

	float3 pos;
	float2 forward_xz;
	float rot_vert;
	float2 xy_offset;
	float zoom;

	void setRotVertLimit(float2);
	Maybe<float2> rot_vert_limit;
};

OrthoCamera lerp(const OrthoCamera &, const OrthoCamera &, float t);
}
