// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx/investigate.h"
#include "fwk/math/box.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {
struct Vis2Label;

struct RenderList; // TODO: remove

class Investigator2 {
  public:
	using Opt = InvestigatorOpt;

	Investigator2(VDeviceRef, VWindowRef, VisFunc2, Maybe<DRect>, InvestigatorOpts);
	~Investigator2();

	void focus(DRect);
	void run();

	void handleInput(float time_diff);
	void applyFocus();
	vector<Vis2Label> draw(RenderList &, TextFormatter &);
	void draw();
	bool mainLoop();
	static bool mainLoop(VulkanWindow &, void *);

  private:
	VDeviceRef m_device;
	VWindowRef m_window;
	VisFunc2 m_vis_func;
	DRect m_focus;

	Dynamic<Font> m_font;
	IRect m_viewport;
	double2 m_view_pos, m_mouse_pos;
	double m_scale = 0.1, m_view_scale = 400.0;
	bool m_exit_please = false;
	bool m_focus_applied = false;
	InvestigatorOpts m_opts;
};
}
