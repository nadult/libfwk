// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/vulkan_device.h"

namespace fwk {

VulkanDevice::VulkanDevice() : m_handle(nullptr) {}
VulkanDevice::VulkanDevice(VkDevice handle, vector<VkQueue> queues,
						   const VulkanPhysicalDeviceInfo &info)
	: m_handle(handle), m_queues(move(queues)), m_info(info) {}

VulkanDevice::~VulkanDevice() {
	if(m_handle)
		vkDestroyDevice(m_handle, nullptr);
}

VulkanDevice::VulkanDevice(VulkanDevice &&rhs)
	: m_handle(rhs.m_handle), m_queues(move(rhs.m_queues)), m_info(move(rhs.m_info)) {
	rhs.m_handle = nullptr;
	rhs.m_info = {};
}
VulkanDevice &VulkanDevice::operator=(VulkanDevice &&rhs) {
	if(m_handle)
		vkDestroyDevice(m_handle, nullptr);
	m_handle = rhs.m_handle;
	m_queues = move(rhs.m_queues);
	rhs.m_handle = nullptr;
	return *this;
}
}
