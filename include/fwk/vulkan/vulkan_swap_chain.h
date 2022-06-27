// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VulkanSurfaceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VulkanSwapChainSetup {
	VkSurfaceFormatKHR surface_format;
	VkPresentModeKHR present_mode;
	VkSurfaceTransformFlagBitsKHR transform;
};

class VulkanSwapChain : public VulkanObjectBase<VulkanSwapChain> {
  public:
	static VulkanSurfaceInfo surfaceInfo(VDeviceRef, VWindowRef);
	static Ex<PVSwapChain> create(VDeviceRef, VWindowRef, const VulkanSwapChainSetup &);

	CSpan<PVImageView> imageViews() const { return m_image_views; }

  private:
	friend class VulkanDevice;
	VulkanSwapChain(VkSwapchainKHR, VObjectId, VWindowRef, vector<PVImageView>);
	~VulkanSwapChain();

	VWindowRef m_window;
	vector<PVImageView> m_image_views;
};
}