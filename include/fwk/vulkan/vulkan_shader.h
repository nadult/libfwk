// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanShaderModule : public VulkanObjectBase<VulkanShaderModule> {
  public:
	static Ex<PVShaderModule> create(VDeviceRef, CSpan<char> bytecode);
	static Ex<PVShaderModule> create(VDeviceRef, CSpan<char> bytecode,
									 vector<DescriptorBindingInfo> bindings);

	CSpan<DescriptorBindingInfo> descriptorBindingInfos() const {
		return m_descriptor_binding_infos;
	}

  private:
	friend class VulkanDevice;
	VulkanShaderModule(VkShaderModule, VObjectId, vector<DescriptorBindingInfo>);
	~VulkanShaderModule();

	vector<DescriptorBindingInfo> m_descriptor_binding_infos;
};

}