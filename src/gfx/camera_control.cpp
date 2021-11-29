// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/camera_control.h"

#include "fwk/any_config.h"
#include "fwk/gfx/camera.h"
#include "fwk/gfx/camera_variant.h"
#include "fwk/math/matrix4.h"
#include "fwk/math/random.h"
#include "fwk/math/ray.h"
#include "fwk/math/segment.h"
#include "fwk/maybe_ref.h"
#include "fwk/sys/input.h"
#include "fwk/variant.h"

namespace fwk {

template <class T> auto timeLerp(T a, T b, float pow, float time_diff) {
	while(time_diff > 0.0f) {
		a = lerp(a, b, pow);
		time_diff -= 1.0 / 240.0;
	}
	return a;
}

struct CameraControl::Impl {
	CameraVariant current, target;
	CameraVariant drag_cam;
};

CameraControl::CameraControl(const Plane3F &base_plane) : m_base_plane(base_plane) {
	m_impl.emplace();
}

FWK_COPYABLE_CLASS_IMPL(CameraControl);

void CameraControl::save(AnyConfig &config) const { config.set("camera_control", target()); }

bool CameraControl::load(const AnyConfig &config) {
	bool loaded = false;
	if(auto cam_conf = config.getMaybe<CameraVariant>("camera_control")) {
		m_impl->current = *cam_conf;
		loaded = true;
	} else {
		m_impl->current = OrthoCamera(float3());
	}

	m_impl->target = m_impl->current;
	return loaded;
}

CameraType CameraControl::type() const { return type(m_impl->current); }
CameraType CameraControl::type(const CameraVariant &cam) { return Type(cam.which()); }

Camera CameraControl::currentCamera() const { return camera(m_impl->current); }
Camera CameraControl::targetCamera() const { return camera(m_impl->target); }

Camera CameraControl::camera(const CameraVariant &cam) const {
	return cam.visit([&](const auto &cam) { return cam.toCamera(o_config.params); });
}

void CameraControl::setTarget(const CameraVariant &cam) {
	bool new_type = type(cam) != type();
	m_impl->target = cam;
	if(new_type)
		m_impl->current = cam;
}

void CameraControl::setTarget(Type type, FBox focus) {
	switch(type) {
	case Type::plane:
		setTarget(PlaneCamera(CamAxis::pos_x, CamAxis::pos_z, focus));
		break;
	case Type::fpp: {
		FppCamera new_cam;
		new_cam.focus(focus);
		setTarget(new_cam);
	} break;
	case Type::ortho: {
		OrthoCamera new_cam;
		new_cam.focus(focus);
		setTarget(new_cam);
	} break;
	case Type::orbiting: {
		OrbitingCamera new_cam;
		new_cam.focus(focus);
		setTarget(new_cam);
	} break;
	}
}

void CameraControl::finishAnim() { m_impl->current = target(); }

const CameraVariant &CameraControl::current() const { return m_impl->current; }
CameraVariant CameraControl::target() const { return m_impl->target; }

static float linearControl(const InputEvent &event, int key1, int key2) {
	float value = 0.0f;
	if(event.keyPressed(key1))
		value -= 1.0f;
	if(event.keyPressed(key2))
		value += 1.0f;
	return value;
}

float CameraControl::moveSpeed() const {
	return lerp(1.0f, o_config.shift_move_speed, m_fast_mul) * o_config.move_multiplier;
}

vector<InputEvent> CameraControl::handleInput(vector<InputEvent> events, MaybeCRef<float3> cursor) {
	float2 move, rot;
	float zoom = 0.0f, mouse_zoom = 0.0f;

	bool lshift = !events.empty() && events.front().pressed(InputModifier::lshift);

	vector<InputEvent> out;
	for(const auto &event : events) {
		if(event.isMouseEvent() &&
		   (!o_config.rotation_filter || (*o_config.rotation_filter)(event)))
			rot = float2(event.mouseMove());

		if(isOneOf(event.key(), 'a', 'd', 'w', 's', 'r', 'f')) {
			move.x += linearControl(event, 'd', 'a');
			move.y += linearControl(event, 's', 'w');
			zoom += linearControl(event, 'r', 'f');
		} else if(isOneOf(event.key(), InputKey::left, InputKey::right, InputKey::up,
						  InputKey::down, InputKey::pageup, InputKey::pagedown)) {
			move.x += linearControl(event, InputKey::right, InputKey::left);
			move.y += linearControl(event, InputKey::down, InputKey::up);
			zoom += linearControl(event, InputKey::pageup, InputKey::pagedown);
		} else if(event.isMouseOverEvent()) {
			mouse_zoom -= event.mouseWheel() * 2.0f;
			out.emplace_back(event);
		} else {
			out.emplace_back(event);
		}
	}

	m_fast_mode = lshift;

	float move_speed = moveSpeed() * m_time_diff;
	move = vclamp(move, float2(-1.0f), float2(1.0f)) * move_speed;
	zoom = clamp(zoom, -1.0f, 1.0f) * 3.0 * move_speed + mouse_zoom * 0.2f;
	rot *= o_config.rotation_speed;

	CameraVariant new_cam = m_impl->target;
	new_cam.visit([&](auto &cam) { cam.move(move, rot, zoom); });

	if(rot != float2() && new_cam.is<OrthoCamera>() && cursor) {
		// Making sure that Ortho cam rotates around cursor
		auto old_pos = mulPoint(camera(m_impl->target).matrix(), *cursor).xy();
		auto new_pos = mulPoint(camera(new_cam).matrix(), *cursor).xy();

		OrthoCamera *ocam = new_cam;
		auto offset = (new_pos - old_pos) * float2(o_config.params.viewport.size()) *
					  float2(-0.5f, 0.5f) / ocam->zoom;
		ocam->xy_offset += offset;
		m_impl->target = new_cam;

	} else {
		m_impl->target = new_cam;
	}
	return out;
}

void CameraControl::tick(double time_diff, bool converge_quickly) {
	m_time_diff = time_diff;
	m_fast_mul = timeLerp(m_fast_mul, m_fast_mode ? 1.0f : 0.0f, 0.04f, time_diff);

	m_impl->target.visit([&](auto &cam) {
		using CamType = Decay<decltype(cam)>;
		CamType *cur_cam = m_impl->current;
		DASSERT(cur_cam);
		*cur_cam = timeLerp(*cur_cam, cam, 0.04f, converge_quickly ? time_diff * 2.0f : time_diff);
	});
}

Segment3<float> CameraControl::screenRay(float2 screen_pos) const {
	return currentCamera().screenRay(screen_pos);
}

Frustum CameraControl::screenFrustum(FRect rect) const {
	return currentCamera().screenFrustum(rect);
}

float3 CameraControl::grabPoint(float3 drag_point, float2 screen_pos) const {
	Camera cam;
	if(FppCamera *fpp_cam = m_impl->target)
		cam =
			FppCamera(drag_point, fpp_cam->forward_xz, fpp_cam->rot_vert).toCamera(o_config.params);
	else if(OrthoCamera *ortho_cam = m_impl->target) {
		cam = ortho_cam->offsetCamera(ortho_cam->xyOffset(drag_point)).toCamera(o_config.params);
	}

	auto segment = cam.screenRay(screen_pos);
	DASSERT(!segment.empty());
	auto ray = *segment.asRay();
	auto isect = ray.isectParam(m_base_plane).closest();
	DASSERT(isect < inf);
	return ray.at(isect);
}

void CameraControl::dragStart() {
	DASSERT(isOneOf(type(), Type::fpp, Type::ortho));
	m_impl->drag_cam = m_impl->target;
}

void CameraControl::drag(float2 mouse_before, float2 mouse_after) {
	if(OrthoCamera *ortho_cam = m_impl->drag_cam) {
		float scale = 1.0f / ortho_cam->zoom;
		setTarget(ortho_cam->offsetCamera((mouse_after - mouse_before) * scale));
	} else if(FppCamera *fpp_cam = m_impl->target) {
		FppCamera *start_cam = m_impl->drag_cam;
		DASSERT(start_cam);
		float3 drag_start = start_cam->pos;
		float3 target = grabPoint(drag_start, mouse_before);
		float3 cur_hit = grabPoint(drag_start, mouse_after);

		float3 cur_pos = drag_start;
		float cur_dist = distance(cur_hit, target);

		Random rand;
		for(int n = 0; n < 500; n++) {
			auto angle = rand.uniform(0.0f, pi * 2.0f);
			float3 vec = asXZY(angleToVector(angle) * cur_dist * 2.0f / 3.0f, 0.0f);
			float3 new_hit = grabPoint(cur_pos + vec, mouse_after);
			float dist = distance(new_hit, target);
			if(dist < cur_dist) {
				cur_pos += vec;
				cur_hit = new_hit;
				cur_dist = dist;
			}
		}

		float max_dist = 150.0f;
		if(distance(cur_pos, drag_start) > max_dist)
			cur_pos = drag_start + normalize(cur_pos - drag_start) * max_dist;

		setTarget(FppCamera(cur_pos, fpp_cam->forward_xz, fpp_cam->rot_vert));
	}
}
}
