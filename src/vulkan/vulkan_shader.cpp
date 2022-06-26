// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_shader.h"

#include "fwk/vulkan/vulkan_instance.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanShaderModule::VulkanShaderModule(VkShaderModule handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanShaderModule ::~VulkanShaderModule() {
	vkDestroyShaderModule(deviceHandle(), m_handle, nullptr);
}

}