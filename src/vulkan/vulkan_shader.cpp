// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_shader.h"

#include "fwk/gfx/shader_compiler.h"
#include "fwk/gfx/shader_reflection.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_pipeline.h"

namespace fwk {

Ex<vector<VDescriptorBindingInfo>> getBindings(CSpan<char> bytecode, VShaderStage &out_stage) {
	auto reflection = EX_PASS(ShaderReflectionModule::create(bytecode));
	vector<VDescriptorBindingInfo> out;
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
									   vector<VDescriptorBindingInfo> infos)
	: VulkanObjectBase(handle, id), m_descriptor_binding_infos(std::move(infos)), m_stage(stage) {}

VulkanShaderModule ::~VulkanShaderModule() {
	vkDestroyShaderModule(deviceHandle(), m_handle, nullptr);
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode,
											  VShaderStage stage,
											  vector<VDescriptorBindingInfo> infos) {
	DASSERT(bytecode.size() % sizeof(u32) == 0);
	DASSERT(u64((void *)bytecode.data()) % sizeof(u32) == 0);

	VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	ci.codeSize = bytecode.size();
	ci.pCode = reinterpret_cast<const u32 *>(bytecode.data());

	VkShaderModule handle;
	FWK_VK_EXPECT_CALL(vkCreateShaderModule, device, &ci, nullptr, &handle);
	return device->createObject(handle, stage, std::move(infos));
}

Ex<PVShaderModule> VulkanShaderModule::create(VDeviceRef device, CSpan<char> bytecode) {
	VShaderStage stage = {};
	auto bindings = EX_PASS(getBindings(bytecode, stage));
	return create(device, bytecode, stage, std::move(bindings));
}

// TODO: move to shader compiler
static string formatBytecode(CSpan<char> bytecode, Str var_name, int max_line_len = 100) {
	TextFormatter fmt;
	fmt("static const u32 % [] = {\n  ", var_name);
	int line_start = fmt.size() - 2;
	auto dwords = bytecode.reinterpret<u32>();
	for(int i : intRange(dwords)) {
		int cur_pos = fmt.size();
		fmt.stdFormat("0x%x, ", dwords[i]);
		if(fmt.size() - line_start > max_line_len) {
			fmt.trim(fmt.size() - cur_pos);
			fmt("\n  ");
			line_start = fmt.size() - 2;
			fmt.stdFormat("0x%x, ", dwords[i]);
		}
	}
	fmt("\n};\n\n");
	return fmt.text();
}

Ex<vector<PVShaderModule>> VulkanShaderModule::compile(ShaderCompiler &compiler, VDeviceRef device,
													   CSpan<Pair<VShaderStage, ZStr>> source_codes,
													   bool dump_bytecodes) {
	vector<PVShaderModule> out;

	// TODO: first compilation is usually slow
	out.reserve(source_codes.size());
	for(auto [stage, code] : source_codes) {
		auto result = EX_PASS(compiler.compileCode(stage, code));
		if(dump_bytecodes) {
			auto text = formatBytecode(result.bytecode, format("%_spirv", stage));
			print("%\n", text);
		}
		out.emplace_back(EX_PASS(create(device, result.bytecode)));
	}

	return out;
}
}