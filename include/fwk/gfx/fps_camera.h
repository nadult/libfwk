// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

struct FpsCamera {
	FpsCamera(float3 pos = float3(), float2 forward_xz = float2(0, 1), float rot_vert = 0.0f)
		: pos(pos), forward_xz(forward_xz), rot_vert(rot_vert) {}

	static Ex<FpsCamera> load(CXmlNode);
	void save(XmlNode) const;

	static FpsCamera closest(const Camera &);
	Camera toCamera(const CameraParams &) const;

	void move(float2 move, float2 rot, float move_up);
	void focus(FBox);

	FWK_ORDER_BY(FpsCamera, pos, forward_xz, rot_vert);

	Pair<float3> forwardRight() const;

	float3 pos;
	float2 forward_xz;
	float rot_vert;
};

FpsCamera lerp(const FpsCamera &, const FpsCamera &, float t);
}
