// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx/investigate.h"
#include "fwk/math/box.h"
#include "fwk/maybe_ref.h"
#include "fwk/vulkan/vulkan_storage.h"

// Parametry:
// - vis_func
// - focus (opcja)
// - flagi
// - identyfikator ?
// - opcje (serializowalne)
//
//
// Problem: identyfikacja:
// - czasami mamy jeden obiekt w grze (singleton, jak np. World3D)
// - wiele investigatorów
// - wiele klas, które są dziećmi jakiejś innej klasy trzymanej w global configu
// - investigator nie bierze shared configu z cameracontrol, ale co najwyżej zwykły config

namespace fwk {

class Investigator3 {
  public:
	using VisFunc = VisFunc3;
	using Opt = InvestigatorOpt;
	using CamVariant = CameraVariant;

	struct Config {
		Maybe<DBox> focus = none;
		MaybeCRef<CamVariant> camera = none;
		float move_speed_multiplier = 1.0;
	};

	Investigator3(VDeviceRef, VWindowRef, VisFunc, InvestigatorOpts, Config);
	~Investigator3();

	void run();

	CamVariant defaultCamera() const;
	CamVariant camera() const;

	void handleInput(vector<InputEvent>, float time_diff);
	void draw();
	bool mainLoop();
	static bool mainLoop(VulkanWindow &, void *);

  private:
	VDeviceRef m_device;
	VWindowRef m_window;
	VisFunc m_vis_func;
	DBox m_focus;
	Dynamic<CameraControl> m_cam_control;

	Dynamic<Font> m_font;
	double2 m_mouse_pos;
	bool m_compute_close = false;
	bool m_exit_please = false;
	InvestigatorOpts m_opts;
};
}
