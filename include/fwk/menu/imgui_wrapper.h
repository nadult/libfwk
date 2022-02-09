// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/gfx_base.h"
#include "fwk/maybe.h"
#include "fwk/menu_base.h"
#include "fwk/vector.h"

namespace fwk {

DEFINE_ENUM(ImGuiStyleMode, normal, mini);

struct ImGuiOptions {
	Maybe<string> font_path;
	Maybe<int> font_size;
	ImGuiStyleMode style_mode = ImGuiStyleMode::normal;
	float dpi_scale = 1.0;
};

class ImGuiWrapper {
  public:
	ImGuiWrapper(GlDevice &, ImGuiOptions = {});
	ImGuiWrapper(ImGuiWrapper &&);
	~ImGuiWrapper();

	void operator=(const ImGuiWrapper &) = delete;

	static ImGuiWrapper *instance() { return s_instance; }

	bool o_hide_menu = false;

	void beginFrame(GlDevice &);
	vector<InputEvent> finishFrame(GlDevice &);
	void drawFrame(GlDevice &);

	AnyConfig config() const;
	void setConfig(const AnyConfig &);

	using ProcessFunc = void (*)(void *);
	void addProcess(ProcessFunc, void *arg);
	void removeProcess(ProcessFunc, void *arg);

  private:
	static const char *getClipboardText(void *);
	static void setClipboardText(void *, const char *);

	struct Process {
		ProcessFunc func;
		void *arg;
	};

	double m_last_time = -1.0;
	vector<Process> m_procs;
	static ImGuiWrapper *s_instance;
};
}
