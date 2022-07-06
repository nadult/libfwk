// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_shader.h"

#include "fwk/gfx/shader_reflection.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_pipeline.h"

#include <vulkan/vulkan.h>

namespace fwk {

Ex<vector<DescriptorBindingInfo>> getBindings(CSpan<char> bytecode) {
	auto reflection = EX_PASS(ShaderReflectionModule::create(bytecode));
	vector<DescriptorBindingInfo> out;
	out.reserve(reflection->descriptor_binding_count);
	uint stage_bits = reflection->shader_stage;
	if(stage_bits > VShaderStageFlags::mask)
		return ERROR("Unsupported shader stage: %", stdFormat("%x", stage_bits));
	VShaderStageFlags stages;
	stages.bits = stage_bits;

	for(int i : intRange(reflection->descriptor_binding_count)) {
		auto &binding = reflection->descriptor_bindings[i];
		uint desc_type = uint(binding.descriptor_type);
		if(desc_type > count<VDescriptorType>)
			return ERROR("Unsupported descriptor type: %", desc_type);
		out.emplace_back(VDescriptorType(desc_type), stages, binding.binding, binding.count,
						 binding.set);
	}

	// TODO: is this needed?
	makeSorted(out);

	return out;
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode,
											  vector<DescriptorBindingInfo> infos) {
	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = bytecode.size();
	// TODO: make sure that this is safe
	ci.pCode = reinterpret_cast<const uint32_t *>(bytecode.data());

	VkShaderModule handle;
	if(vkCreateShaderModule(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateShaderModule failed");
	return device->createObject(handle, move(infos));
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode) {
	auto bindings = EX_PASS(getBindings(bytecode));
	return create(device, bytecode, move(bindings));
}

VulkanShaderModule::VulkanShaderModule(VkShaderModule handle, VObjectId id,
									   vector<DescriptorBindingInfo> infos)
	: VulkanObjectBase(handle, id), m_descriptor_binding_infos(move(infos)) {}

VulkanShaderModule ::~VulkanShaderModule() {
	vkDestroyShaderModule(deviceHandle(), m_handle, nullptr);
}

}