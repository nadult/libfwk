// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include "fwk/str.h"
#include "fwk/vector.h"

namespace fwk {

class InputState;
class InputEvent;

// TODO: separate window flags from device flags
DEFINE_ENUM(VDeviceFlag, fullscreen, fullscreen_desktop, resizable, centered, vsync, maximized,
			allow_hidpi, validation);
using VDeviceFlags = EnumFlags<VDeviceFlag>;

struct VulkanDeviceConfig {
	VDeviceFlags flags;
	double version = 3.1;
	Maybe<int> multisampling = none;
};

class VulkanDevice {
  public:
	VulkanDevice();
	~VulkanDevice();

	using Flag = VDeviceFlag;
	using Flags = VDeviceFlags;
	using Config = VulkanDeviceConfig;

	static bool isPresent();
	static VulkanDevice &instance();

	vector<IRect> displayRects() const;
	vector<float> displayDpiScales() const;

	// If window is outside of all display rects then it's positioned on first display
	static IRect sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
									float minimum_overlap = 0.1);

	void createWindow(ZStr title, IRect rect, Config = {});
	void destroyWindow();
	void printDeviceInfo();

	int swapInterval();
	void setSwapInterval(int);

	void setWindowTitle(ZStr);
	void setWindowSize(const int2 &);
	int2 windowSize() const;

	void setWindowRect(IRect);
	IRect windowRect() const;
	IRect restoredWindowRect() const;
	EnumMap<RectSide, int> windowBorder() const;

	int windowDisplayIndex() const;
	float windowDpiScale() const;
	Flags windowFlags() const;

	// Only accepted: none, fullscreen or fullscreen_desktop
	void setWindowFullscreen(Flags);

	bool isWindowFullscreen() const;
	bool isWindowMaximized() const;

	double frameTime() const { return m_frame_time; }

	void grabMouse(bool);
	void showCursor(bool);

	const InputState &inputState() const;
	const vector<InputEvent> &inputEvents() const;

	using MainLoopFunction = bool (*)(VulkanDevice &device, void *argument);
	void runMainLoop(MainLoopFunction, void *argument = nullptr);

	string clipboardText() const;
	void setClipboardText(ZStr);

	PProgram cacheFindProgram(Str) const;
	void cacheAddProgram(Str, PProgram);

  private:
	bool pollEvents();

	vector<Pair<MainLoopFunction, void *>> m_main_loop_stack;

	struct InputImpl;
	Dynamic<InputImpl> m_input_impl;
	double m_last_time, m_frame_time;

	struct WindowImpl;
	Dynamic<WindowImpl> m_window_impl;
};
}
