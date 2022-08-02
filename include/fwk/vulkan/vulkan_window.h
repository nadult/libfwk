// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

// TODO: cleanup in includes

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/str.h"
#include "fwk/vector.h"
#include "fwk/vulkan/vulkan_storage.h"
#include "fwk/vulkan_base.h"

namespace fwk {

class InputState;
class InputEvent;

DEFINE_ENUM(VWindowFlag, fullscreen, fullscreen_desktop, resizable, centered, maximized, minimized,
			allow_hidpi, sleep_when_minimized);
using VWindowFlags = EnumFlags<VWindowFlag>;

DECLARE_ENUM(WindowEvent);

class VulkanWindow {
  public:
	using Flag = VWindowFlag;

	static Ex<VWindowRef> create(VInstanceRef, ZStr title, IRect rect, VWindowFlags);

	VkSurfaceKHR surfaceHandle() const;

	static vector<IRect> displayRects();
	static vector<float> displayDpiScales();

	// If window is outside of all display rects then it's positioned on first display
	static IRect sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
									float minimum_overlap = 0.1);

	void setTitle(ZStr);
	void setSize(const int2 &);
	int2 size() const;

	void setRect(IRect);
	IRect rect() const;
	IRect restoredRect() const;
	EnumMap<RectSide, int> border() const;

	int displayIndex() const;
	float dpiScale() const;

	// Only accepted: none, fullscreen or fullscreen_desktop
	void setFullscreen(VWindowFlags);

	bool isFullscreen() const { return flags() & (Flag::fullscreen | Flag::fullscreen_desktop); }

	// Some of this state is only updated during pollEvents()
	VWindowFlags flags() const;
	bool isMaximized() const { return flags() & Flag::maximized; }
	bool isMinimized() const { return flags() & Flag::minimized; }

	Maybe<double> fps() const;

	void grabMouse(bool);
	void showCursor(bool);

	string clipboardText() const;
	void setClipboardText(ZStr);

	const InputState &inputState() const;
	const vector<InputEvent> &inputEvents() const;

	using MainLoopFunction = bool (*)(VulkanWindow &window, void *argument);
	void runMainLoop(MainLoopFunction, void *argument = nullptr);

  private:
	friend VulkanStorage;
	VulkanWindow(VWindowId, VInstanceRef);
	~VulkanWindow();

	Ex<void> initialize(ZStr title, IRect, VWindowFlags);

	VulkanWindow(const VulkanWindow &) = delete;
	void operator=(VulkanWindow &) = delete;

	bool pollEvents();
	void updateFPS(bool reset = false);

	vector<Pair<MainLoopFunction, void *>> m_main_loop_stack;

	struct Impl;
	Dynamic<Impl> m_impl;
};
}
