// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx/investigate.h"
#include "fwk/math/box.h"

namespace fwk {
struct Vis2Label;

class Investigator2 {
  public:
	using Opt = InvestigatorOpt;

	Investigator2(VisFunc2, Maybe<DRect>, InvestigatorFlags);
	~Investigator2();

	void focus(DRect);
	void run();

	void handleInput(GlDevice &, float time_diff);
	void applyFocus();
	vector<Vis2Label> draw(RenderList &, TextFormatter &);
	void draw();
	bool mainLoop(GlDevice &);
	static bool mainLoop(GlDevice &, void *);

  private:
	VisFunc2 m_vis_func;
	DRect m_focus;

	Dynamic<Font> m_font;
	IRect m_viewport;
	double2 m_view_pos, m_mouse_pos;
	double m_scale = 0.1, m_view_scale = 400.0;
	bool m_esc_pressed = false;
	bool m_focus_applied = false;
	InvestigatorFlags m_flags;
};
}
