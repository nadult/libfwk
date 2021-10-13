// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/gfx_base.h"
#include "fwk/menu_base.h"
#include "fwk/vector.h"

namespace fwk {

DEFINE_ENUM(ImGuiStyleMode, normal, mini);

class ImGuiWrapper {
  public:
	ImGuiWrapper(GlDevice &, ImGuiStyleMode);
	ImGuiWrapper(ImGuiWrapper &&);
	~ImGuiWrapper();

	void operator=(const ImGuiWrapper &) = delete;

	static ImGuiWrapper *instance() { return s_instance; }

	bool o_hide_menu = false;

	void beginFrame(GlDevice &);
	vector<InputEvent> finishFrame(GlDevice &);
	void drawFrame(GlDevice &);

	void saveSettings(XmlNode) const;
	void loadSettings(CXmlNode) NOEXCEPT;

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
