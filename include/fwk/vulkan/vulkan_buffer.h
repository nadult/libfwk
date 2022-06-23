// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_ptr.h"

namespace fwk {

class VulkanBuffer {
  public:
	VulkanBuffer(u64 size) : m_size(size) {}
	~VulkanBuffer() = default;

	static Ex<VPtr<VkBuffer>> make(VDeviceId, u64 size);

	//static Ex<PShaderModule> make(VDeviceId, ShaderType, ZStr code);
	//static Ex<PShaderModule> load(VDeviceId, ZStr file_name);

  private:
	u64 m_size;
};

}