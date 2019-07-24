// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/ortho_camera.h"

#include "fwk/gfx/camera.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/xml.h"

namespace fwk {

Ex<OrthoCamera> OrthoCamera::load(CXmlNode node) {
	OrthoCamera out{node.attrib<float3>("pos"), node.attrib<float2>("forward_xz"),
					node.attrib<float>("rot_vert"), node.attrib<float2>("xy_offset"),
					node.attrib<float>("zoom")};
	EXPECT_CATCH();
	return out;
}

void OrthoCamera::save(XmlNode node) const {
	node.addAttrib("pos", pos);
	node.addAttrib("forward_xz", forward_xz);
	node.addAttrib("rot_vert", rot_vert);
	node.addAttrib("xy_offset", xy_offset);
	node.addAttrib("zoom", zoom);
}

float3 OrthoCamera::center() const {
	auto [fwd, right] = forwardRight();
	auto up = cross(fwd, right);
	return pos + right * xy_offset.x + up * xy_offset.y;
}

OrthoCamera OrthoCamera::closest(const Camera &cam) {
	auto fwd = cam.forward();
	auto fwd_xz = fwd.xz();

	float best = 0.0;
	float best_dist = inf;
	for(int n = 0; n < 1024; n++) {
		float ang = float(n) * pi * 1.99f / float(1023) - pi;
		OrthoCamera temp(cam.pos(), fwd_xz, ang);
		auto fwd_right = temp.forwardRight();
		float dist = distance(fwd_right.first, fwd);
		if(dist < best_dist) {
			best_dist = dist;
			best = ang;
		}
	}

	return {cam.pos(), cam.forward().xz(), best};
}
Camera OrthoCamera::toCamera(const CameraParams &params) const {
	auto [fwd, right] = forwardRight();
	auto up = cross(fwd, right);
	auto tparams = params;
	tparams.ortho_scale *= zoom;
	tparams.depth = tparams.depth * zoom;

	double2 pixel_size = double2(1.0f / tparams.ortho_scale);

	// TODO: zaokrąglanie powinno być przeniesione gdzieś indziej
	// TODO: zaokrąglanie powinniśmy robić tylko jak przesuwamy kamerę, jak się zoomujemy
	// to nie ma to sensu
	auto xy_pos = vround(double2(xy_offset) / pixel_size) * pixel_size;
	auto tpos = pos + right * xy_pos.x + up * xy_pos.y;
	auto out = Camera(tpos, tpos + fwd * 10.0f, up, tparams);
	return out;
}

void OrthoCamera::applyXYOffset() {
	auto [fwd, right] = forwardRight();
	auto up = cross(fwd, right);
	pos += right * xy_offset.x + up * xy_offset.y;
	xy_offset = {};
}

float2 OrthoCamera::xyOffset(float3 point) const {
	auto [fwd, right] = forwardRight();
	auto up = cross(fwd, right);

	auto vec = point - pos;
	return float2(dot(vec, right), dot(vec, up)) - xy_offset;
}

pair<float3, float3> OrthoCamera::forwardRight() const {
	float3 right = normalize(asXZY(-perpendicular(forward_xz), 0.0f));
	auto forward = normalize(rotateVector(asXZY(forward_xz, 0.0f), right, rot_vert));
	return {forward, right};
}

void OrthoCamera::move(float2 move, float2 rot, float move_up) {
	// TODO: pass time_diff as well
	float pi_half = pi * 0.5f;

	// TODO: rotation around cursor
	if(rot != float2())
		applyXYOffset();

	move *= 500.0f / zoom;
	rot *= 0.5f;

	auto new_fwd = rotateVector(forward_xz, rot.x);
	auto new_vert = clamp(rot_vert + rot.y, -pi_half, pi_half);
	auto new_zoom = clamp(zoom * (1.0f - move_up * 0.1f), 5.0f, 200.0f);

	xy_offset += move;
	zoom = new_zoom;
	forward_xz = new_fwd;
	rot_vert = new_vert;
}

void OrthoCamera::focus(FBox box) {
	// TODO: take current position into consideration ?

	auto center = box.center();
	// TODO: use frustum to compute it accurately
	float dist = max(box.size().values()) * 1.25f;
	pos = center - float3(0, 0, dist);
	forward_xz = {0, 1};
	rot_vert = 0;
}

OrthoCamera lerp(const OrthoCamera &lhs, const OrthoCamera &rhs, float t) {
	return {lerp(lhs.pos, rhs.pos, t), lerp(lhs.forward_xz, rhs.forward_xz, t),
			fwk::lerp(lhs.rot_vert, rhs.rot_vert, t), lerp(lhs.xy_offset, rhs.xy_offset, t),
			fwk::lerp(lhs.zoom, rhs.zoom, t)};
}
}
