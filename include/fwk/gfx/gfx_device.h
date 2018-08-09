// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"
#include "fwk/str.h"
#include "fwk/sys/unique_ptr.h"

namespace fwk {

class InputState;
class InputEvent;

DEFINE_ENUM(GfxDeviceOpt, multisampling, fullscreen, fullscreen_desktop, resizable, centered, vsync,
			maximized, opengl_debug_handler);
using GfxDeviceFlags = EnumFlags<GfxDeviceOpt>;

class GfxDevice {
  public:
	GfxDevice();
	~GfxDevice();

	static GfxDevice &instance();
	using Opt = GfxDeviceOpt;
	using Flags = GfxDeviceFlags;

	void createWindow(const string &name, const int2 &size, Flags,
					  OpenglProfile profile = OpenglProfile::core, double opengl_version = 3.1);
	void destroyWindow();
	void printDeviceInfo();

	void setWindowSize(const int2 &);
	int2 windowSize() const;

	void setWindowFullscreen(Flags);
	Flags windowFlags() const;
	bool isWindowFullscreen() const {
		return (bool)(windowFlags() & (Opt::fullscreen | Opt::fullscreen_desktop));
	}

	double frameTime() const { return m_frame_time; }

	void grabMouse(bool);
	void showCursor(bool);

	const InputState &inputState() const;
	const vector<InputEvent> &inputEvents() const;

	using MainLoopFunction = bool (*)(GfxDevice &device, void *argument);
	void runMainLoop(MainLoopFunction, void *argument = nullptr);

	static void clearColor(FColor);
	static void clearDepth(float depth_value);

	string extensions() const;

	string clipboardText() const;
	void setClipboardText(ZStr);

  private:
	bool pollEvents();

#ifdef __EMSCRIPTEN__
	static void emscriptenCallback();
#endif
	vector<pair<MainLoopFunction, void *>> m_main_loop_stack;

	struct InputImpl;
	UniquePtr<InputImpl> m_input_impl;
	double m_last_time, m_frame_time;

	struct WindowImpl;
	UniquePtr<WindowImpl> m_window_impl;
};
}
