// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/plane_camera.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/camera.h"
#include "fwk/sys/xml.h"

namespace fwk {

static const EnumMap<CamAxis, float3> axes = {
	{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}}};

float3 vec(CamAxis axis) { return axes[axis]; }
CamAxis negate(CamAxis axis) { return CamAxis(((int)axis + 3) % 6); }

PlaneCamera::PlaneCamera(Axis ax1, Axis ax2, double3 pos, double zoom)
	: pos(pos), zoom(zoom), axis_right(ax1), axis_up(ax2) {}

PlaneCamera::PlaneCamera(Axis ax1, Axis ax2, FBox focus) : PlaneCamera(ax1, ax2) {
	auto center = focus.center();
	pos = center;

	zoom = 1024 / max(focus.width(), focus.height(), focus.depth());
	if(zoom == 0.0)
		zoom = 1.0f;
}

Ex<PlaneCamera> PlaneCamera::load(CXmlNode node) {
	return PlaneCamera{node.attrib<Axis>("axis1"), node.attrib<Axis>("axis2"),
					   node.attrib("pos", double3()), node.attrib("zoom", 1.0)};
}

void PlaneCamera::save(XmlNode node) const {
	node.addAttrib("pos", pos, double3());
	node.addAttrib("zoom", zoom, 1.0);
	node.addAttrib("axis1", axis_right);
	node.addAttrib("axis2", axis_up);
}

float3 PlaneCamera::up() const { return vec(axis_up); }
float3 PlaneCamera::forward() const { return cross(right(), up()); }
float3 PlaneCamera::right() const { return vec(axis_right); }

PlaneCamera lerp(const PlaneCamera &a, const PlaneCamera &b, float t) {
	return {a.axis_right, a.axis_up, fwk::lerp(a.pos, b.pos, t), fwk::lerp(a.zoom, b.zoom, t)};
}

Camera PlaneCamera::toCamera(const CameraParams &params) const {
	auto tparams = params;
	tparams.projection = CameraProjection::orthogonal;
	tparams.ortho_scale = zoom;
	float3 fpos(pos);
	return Camera(fpos, fpos + forward(), up(), tparams);
}

void PlaneCamera::move(float2 move, float2 rot, float scale_mod) {
	float new_zoom = zoom;
	if(scale_mod != 0.0) {
		scale_mod = clamp(scale_mod, -0.99f, 0.99f) * (1.0f / 10.0f);
		new_zoom = clamp(new_zoom * (1.0 - scale_mod), 0.000000000001, 1000000000000.0);
	}

	pos += double3(move.x * right() + move.y * up()) * (600.0 / new_zoom);
	zoom = new_zoom;
}
}
