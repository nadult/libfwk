// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/vulkan_instance.h"

namespace fwk {

class VulkanDevice {
  public:
	VulkanDevice();
	VulkanDevice(VkDevice, vector<VkQueue>, const VulkanPhysicalDeviceInfo &);
	FWK_MOVABLE_CLASS(VulkanDevice);

	VkDevice handle() const { return m_handle; }
	const auto &physicalDeviceInfo() const { return m_info; }

  private:
	VkDevice m_handle;
	VkPhysicalDevice m_phys_device;
	vector<VkQueue> m_queues;
	VulkanPhysicalDeviceInfo m_info;
};
}
