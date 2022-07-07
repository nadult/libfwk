// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_shader.h"

#include "fwk/gfx/shader_compiler.h"
#include "fwk/gfx/shader_reflection.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_pipeline.h"

#include <vulkan/vulkan.h>

namespace fwk {

Ex<vector<DescriptorBindingInfo>> getBindings(CSpan<char> bytecode, VShaderStage &out_stage) {
	auto reflection = EX_PASS(ShaderReflectionModule::create(bytecode));
	vector<DescriptorBindingInfo> out;
	out.reserve(reflection->descriptor_binding_count);
	uint stage_bits = reflection->shader_stage;
	if(countBits(stage_bits) > 1)
		return ERROR("Currently ShaderModule only supports single shader stage");
	uint stage_num = countTrailingZeros(stage_bits);
	if(stage_num >= count<VShaderStage>)
		return ERROR("Unsupported shader stage: %", stage_num);
	VShaderStage stage = VShaderStage(stage_num);
	out_stage = stage;

	for(int i : intRange(reflection->descriptor_binding_count)) {
		auto &binding = reflection->descriptor_bindings[i];
		uint desc_type = uint(binding.descriptor_type);
		if(desc_type > count<VDescriptorType>)
			return ERROR("Unsupported descriptor type: %", desc_type);
		out.emplace_back(VDescriptorType(desc_type), flag(stage), binding.binding, binding.count,
						 binding.set);
	}

	// TODO: is this needed?
	makeSorted(out);

	return out;
}

VulkanShaderModule::VulkanShaderModule(VkShaderModule handle, VObjectId id, VShaderStage stage,
									   vector<DescriptorBindingInfo> infos)
	: VulkanObjectBase(handle, id), m_descriptor_binding_infos(move(infos)), m_stage(stage) {}

VulkanShaderModule ::~VulkanShaderModule() {
	vkDestroyShaderModule(deviceHandle(), m_handle, nullptr);
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode,
											  VShaderStage stage,
											  vector<DescriptorBindingInfo> infos) {
	DASSERT(bytecode.size() % sizeof(u32) == 0);
	DASSERT(u64((void *)bytecode.data()) % sizeof(u32) == 0);

	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = bytecode.size();
	ci.pCode = reinterpret_cast<const u32 *>(bytecode.data());

	VkShaderModule handle;
	if(vkCreateShaderModule(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateShaderModule failed");
	return device->createObject(handle, stage, move(infos));
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode) {
	VShaderStage stage = {};
	auto bindings = EX_PASS(getBindings(bytecode, stage));
	return create(device, bytecode, stage, move(bindings));
}

Ex<vector<PVShaderModule>>
VulkanShaderModule::compile(VDeviceRef device, CSpan<Pair<VShaderStage, ZStr>> source_codes) {
	vector<PVShaderModule> out;
	ShaderCompiler compiler;

	out.reserve(source_codes.size());
	for(auto [stage, code] : source_codes) {
		auto result = compiler.compile(stage, code);
		if(!result.bytecode)
			return ERROR("Failed to compile '%' shader:\n%", stage, result.messages);
		out.emplace_back(EX_PASS(create(device, result.bytecode)));
	}

	return out;
}
}