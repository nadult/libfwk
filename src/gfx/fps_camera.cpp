// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/fps_camera.h"

#include "fwk/gfx/camera.h"
#include "fwk/sys/xml.h"

namespace fwk {

FpsCamera lerp(const FpsCamera &a, const FpsCamera &b, float t) {
	return FpsCamera(fwk::lerp(a.pos, b.pos, t), fwk::lerp(a.forward_xz, b.forward_xz, t),
					 fwk::lerp(a.rot_vert, b.rot_vert, t));
}

Ex<FpsCamera> FpsCamera::load(CXmlNode node) {
	return FpsCamera{node.attrib<float3>("pos"), node.attrib<float2>("forward_xz"),
					 node.attrib<float>("rot_vert")};
}

void FpsCamera::save(XmlNode node) const {
	node.addAttrib("pos", pos);
	node.addAttrib("forward_xz", forward_xz);
	node.addAttrib("rot_vert", rot_vert);
}

Pair<float3> FpsCamera::forwardRight() const {
	float3 right = asXZY(-perpendicular(forward_xz), 0.0f);
	auto forward = rotateVector(asXZY(forward_xz, 0.0f), right, rot_vert);
	return {forward, right};
}

FpsCamera FpsCamera::closest(const Camera &cam) {
	auto fwd = cam.forward();
	auto fwd_xz = fwd.xz();

	float best = 0.0;
	float best_dist = inf;
	for(int n = 0; n < 1024; n++) {
		float ang = float(n) * pi * 1.99f / float(1023) - pi;
		FpsCamera temp(cam.pos(), fwd_xz, ang);
		auto fwd_right = temp.forwardRight();
		float dist = distance(fwd_right.first, fwd);
		if(dist < best_dist) {
			best_dist = dist;
			best = ang;
		}
	}

	return {cam.pos(), cam.forward().xz(), best};
}

Camera FpsCamera::toCamera(const CameraParams &params) const {
	auto fwd_right = forwardRight();
	auto up = cross(fwd_right.first, fwd_right.second);
	return Camera(pos, pos + fwd_right.first * 10.0f, up, params);
}

void FpsCamera::move(float2 move, float2 rot, float move_up) {
	float pi_half = pi * 0.5f;

	move *= 16.0f;
	move_up *= -2.50f;
	rot *= 0.5f;

	auto new_fwd = rotateVector(forward_xz, rot.x);
	auto new_vert = clamp(rot_vert + rot.y, -pi_half, pi_half);

	auto fwd_right = forwardRight();
	auto up = cross(fwd_right.first, fwd_right.second);

	pos += fwd_right.second * move.x + fwd_right.first * move.y + up * move_up;
	forward_xz = new_fwd;
	rot_vert = new_vert;
}

void FpsCamera::focus(FBox box) {
	// TODO: take current position into consideration ?

	auto center = box.center();
	// TODO: use frustum to compute it accurately
	float dist = max(box.size().values()) * 1.25f;
	pos = center - float3(0, 0, dist);
	forward_xz = {0, 1};
	rot_vert = 0;
}
}
