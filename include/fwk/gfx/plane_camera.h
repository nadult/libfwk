// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(CamAxis, pos_x, pos_y, pos_z, neg_x, neg_y, neg_z);
float3 vec(CamAxis);
CamAxis negate(CamAxis);

// Mamy obiekty 2D, które można zrzutować na różne płaszczyzny
// I dodatkowo mamy wizualizację 2D w różnych płaszczyznach

struct PlaneCamera {
	using Axis = CamAxis;

	PlaneCamera(Axis ax_right, Axis ax_up, double3 pos = float3(), double zoom = 1.0);
	PlaneCamera(Axis, Axis, FBox);

	static Ex<PlaneCamera> load(CXmlNode);
	void save(XmlNode) const;

	float3 up() const;
	float3 forward() const;
	float3 right() const;

	Camera toCamera(const CameraParams &) const;
	void move(float2 move, float2 rot, float zoom);

	FWK_ORDER_BY(PlaneCamera, pos, zoom, axis_right, axis_up);

	double3 pos;
	double zoom;
	Axis axis_right, axis_up;
};

PlaneCamera lerp(const PlaneCamera &, const PlaneCamera &, float t);
}
