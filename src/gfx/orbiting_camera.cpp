// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/orbiting_camera.h"

#include "fwk/gfx/camera.h"
#include "fwk/sys/xml.h"

namespace fwk {
OrbitingCamera::OrbitingCamera(float3 center, float distance, float rot_horiz, float rot_vert)
	: center(center), distance(distance), rot_horiz(rot_horiz), rot_vert(rot_vert) {
	DASSERT(distance > 0.0f);
}

Ex<OrbitingCamera> OrbitingCamera::load(CXmlNode node) {
	OrbitingCamera out{node.attrib<float3>("center"), node.attrib<float>("distance"),
					   node.attrib<float>("rot_horiz"), node.attrib<float>("rot_vert")};
	EXPECT_CATCH();
	return out;
}

void OrbitingCamera::save(XmlNode node) const {
	node.addAttrib("center", center);
	node.addAttrib("distance", distance);
	node.addAttrib("rot_horiz", rot_horiz);
	node.addAttrib("rot_vert", rot_vert);
}

OrbitingCamera lerp(const OrbitingCamera &a, const OrbitingCamera &b, float t) {
	return {fwk::lerp(a.center, b.center, t), fwk::lerp(a.distance, b.distance, t),
			fwk::lerp(a.rot_horiz, b.rot_horiz, t), fwk::lerp(a.rot_vert, b.rot_vert, t)};
}

array<float3, 2> OrbitingCamera::forwardRight() const {
	float3 forward = asXZY(rotateVector(float2(0, 1), rot_horiz), 0.0f);
	float3 right = asXZY(-perpendicular(forward.xz()), 0.0f);
	forward = rotateVector(forward, right, rot_vert);
	return {{forward, right}};
}

Camera OrbitingCamera::toCamera(const CameraParams &params) const {
	auto fwd_right = forwardRight();
	float3 pos = center - fwd_right[0] * distance;
	auto up = cross(fwd_right[0], fwd_right[1]);
	return Camera(pos, center, up, params);
}

void OrbitingCamera::focus(FBox box) {
	center = box.center();
	distance = max(box.width(), box.height(), box.depth()) * 1.25f;
}

void OrbitingCamera::move(float2 move, float2 rot, float tdist) {
	float pi_half = pi * 0.5f;

	move *= 20.0f;
	rot *= 0.5f;

	auto new_horiz = rot_horiz + rot.x;
	auto new_vert = clamp(rot_vert + rot.y, -pi_half, pi_half);
	auto fwd_right = forwardRight();

	auto new_dist = clamp(distance + tdist * sqrtf(distance), 0.01f, 1000.0f);
	center += fwd_right[0] * move.y + fwd_right[1] * move.x;
	distance = new_dist;
	rot_horiz = new_horiz;
	rot_vert = new_vert;
}
}
