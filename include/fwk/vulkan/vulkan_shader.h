// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanShaderModule : public VulkanObjectBase<VulkanShaderModule> {
  public:
	static Ex<PVShaderModule> create(VulkanDevice &, CSpan<char> bytecode);
	static Ex<PVShaderModule> create(VulkanDevice &, CSpan<char> bytecode, VShaderStage stage,
									 vector<VDescriptorBindingInfo> bindings);

	static Ex<vector<PVShaderModule>> compile(ShaderCompiler &, VulkanDevice &,
											  CSpan<Pair<VShaderStage, ZStr>> source_codes,
											  bool dump_bytecodes = false);

	VShaderStage stage() const { return m_stage; }
	CSpan<VDescriptorBindingInfo> descriptorBindingInfos() const {
		return m_descriptor_binding_infos;
	}

  private:
	friend class VulkanDevice;
	VulkanShaderModule(VkShaderModule, VObjectId, VShaderStage, vector<VDescriptorBindingInfo>);
	~VulkanShaderModule();

	vector<VDescriptorBindingInfo> m_descriptor_binding_infos;
	VShaderStage m_stage;
};

}