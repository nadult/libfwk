// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanShaderModule : public VulkanObjectBase<VulkanShaderModule> {
  public:
  private:
	friend class VulkanDevice;
	VulkanShaderModule(VkShaderModule, VObjectId);
	~VulkanShaderModule();
};

}