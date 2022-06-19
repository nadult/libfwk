// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

// TODO: cleanup in includes

#include "vulkan/vulkan.h"

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include "fwk/str.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

class InputState;
class InputEvent;

// TODO: separate window flags from device flags
DEFINE_ENUM(VWindowFlag, fullscreen, fullscreen_desktop, resizable, centered, vsync, maximized,
			allow_hidpi);
using VWindowFlags = EnumFlags<VWindowFlag>;

struct VulkanWindowConfig {
	VWindowFlags flags;
};

struct VulkanSurfaceDeviceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VulkanSwapChainInfo {
	VkSwapchainKHR handle;
	VDeviceId device_id;
	vector<VkImage> images;
	vector<VkImageView> image_views;
	VkFormat format;
	VkExtent2D extent;
};

class VulkanWindow {
  public:
	using Flag = VWindowFlag;
	using Flags = VWindowFlags;
	using Config = VulkanWindowConfig;

	VulkanWindow();
	FWK_MOVABLE_CLASS(VulkanWindow);

	Ex<void> exConstruct(ZStr title, IRect rect, Config);
	Ex<void> createSwapChain(VDeviceId);

	VkSurfaceKHR surfaceHandle() const;
	VulkanSurfaceDeviceInfo surfaceDeviceInfo(VPhysicalDeviceId) const;
	const VulkanSwapChainInfo &swapChain() const { return *m_swap_chain; }

	vector<IRect> displayRects() const;
	vector<float> displayDpiScales() const;

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
	Flags flags() const;

	// Only accepted: none, fullscreen or fullscreen_desktop
	void setFullscreen(Flags);

	bool isFullscreen() const;
	bool isMaximized() const;

	double frameTime() const;

	void grabMouse(bool);
	void showCursor(bool);

	const InputState &inputState() const;
	const vector<InputEvent> &inputEvents() const;

	using MainLoopFunction = bool (*)(VulkanWindow &window, void *argument);
	void runMainLoop(MainLoopFunction, void *argument = nullptr);

  private:
	bool pollEvents();

	vector<Pair<MainLoopFunction, void *>> m_main_loop_stack;

	struct Impl;
	Dynamic<Impl> m_impl;
	Dynamic<VulkanSwapChainInfo> m_swap_chain;
};
}
