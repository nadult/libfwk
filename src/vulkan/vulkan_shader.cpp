// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_shader.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "vulkan/vulkan.h"

namespace fwk {

//VulkanShaderModule::VulkanShaderModule() = default;
//VulkanShaderModule::~VulkanShaderModule() = default;

/*
Ex<PShaderModule> VulkanShaderModule::make(VDeviceId device_id, ShaderType type, ZStr code) {
	auto &vulkan = VulkanInstance::instance();
	VkShaderModuleCreateInfo ci{};
	VkShaderModule handle = 0;
	auto result = vkCreateShaderModule(vulkan[device_id].handle, &ci, nullptr, &handle);
	if(result != VK_SUCCESS)
		return ERROR("Error when creating vulkan shader module");

	return {};
}

Ex<PShaderModule> VulkanShaderModule::load(VDeviceId device_id, ZStr file_name) { return {}; }
*/

}