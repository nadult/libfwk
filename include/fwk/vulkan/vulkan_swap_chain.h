// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VulkanSurfaceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VulkanSwapChainSetup {
	vector<VkFormat> preferred_formats = {VK_FORMAT_B8G8R8A8_SRGB};
	VPresentMode preferred_present_mode = VPresentMode::fifo;
};

class VulkanSwapChain : public VulkanObjectBase<VulkanSwapChain> {
  public:
	static VulkanSurfaceInfo surfaceInfo(VDeviceRef, VWindowRef);
	static PVSwapChain create(VDeviceRef, VWindowRef, const VulkanSwapChainSetup &);

	CSpan<PVImageView> imageViews() const { return m_image_views; }
	void recreate();

  private:
	friend class VulkanDevice;
	VulkanSwapChain(VkSwapchainKHR, VObjectId, VWindowRef);
	~VulkanSwapChain();

	void initialize();

	VWindowRef m_window;
	VulkanSwapChainSetup m_setup;
	vector<PVImageView> m_image_views;
};
}