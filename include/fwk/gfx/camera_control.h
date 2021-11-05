// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/gfx/camera.h"
#include "fwk/math/frustum.h"
#include "fwk/math/plane.h"
#include "fwk/maybe_ref.h"

namespace fwk {

// TODO: nazewnictwo: odroznic Camera od GameCam, OrbitingCam, ...
//       nazwy pozostałych klas tez mozna poprawic...
// TODO: kamera powinna zwracać informacje o wielkości piksela / jaką wielkość powinny
//       mieć punkty, żeby na ekranie dać 1 pixel?

class CameraControl {
  public:
	using Type = CameraType;

	CameraControl(const Plane3F &base_plane);
	FWK_COPYABLE_CLASS(CameraControl);

	void save(AnyConfig &) const;
	bool load(const AnyConfig &);

	using InputFilter = bool (*)(const InputEvent &);

	struct Config {
		Config() = default;

		FWK_ORDER_BY(Config, params, move_multiplier, shift_move_speed, rotation_filter);

		CameraParams params;
		float move_multiplier = 1.0f;
		float shift_move_speed = 2.0f;
		float rotation_speed = 1.0 / 60.0;
		Maybe<InputFilter> rotation_filter = none;
	};
	Config o_config;

	// In different use-cases we have small differences in the way controls are handled...
	vector<InputEvent> handleInput(vector<InputEvent>, MaybeCRef<float3> cursor = none);
	void tick(double time_diff, bool converge_quickly);

	void setTarget(const CameraVariant &);
	void setTarget(Type, FBox);

	void finishAnim();

	const CameraVariant &current() const;
	CameraVariant target() const;

	Camera currentCamera() const;
	Camera targetCamera() const;
	Camera camera(const CameraVariant &) const;
	// TODO: template should be better

	Type type() const;
	static Type type(const CameraVariant &);

	Segment3<float> screenRay(float2 screen_pos) const;
	Frustum screenFrustum(FRect rect) const;

	void dragStart();
	void drag(float2 mouse_before, float2 mouse_after);

  private:
	float3 grabPoint(float3 drag_point, float2 screen_pos) const;

	struct Impl;
	Dynamic<Impl> m_impl;
	Plane3F m_base_plane;
	double m_time_diff = 0.0;

	float moveSpeed() const;
	bool m_fast_mode = false;
	float m_fast_mul = 0.0f;
};
}
