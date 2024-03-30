// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_storage.h"
#include "fwk/vulkan_base.h"

namespace fwk {

DEFINE_ENUM(VAccelStructType, top_level, bottom_level);

class VulkanAccelStruct : public VulkanObjectBase<VulkanAccelStruct> {
  public:
	static Ex<PVAccelStruct> create(VulkanDevice &, VAccelStructType, VBufferSpan<char>);

	static Ex<PVAccelStruct> build(VulkanDevice &, VBufferSpan<float3> vertices,
								   VBufferSpan<u32> indices);

  private:
	friend class VulkanDevice;
	VulkanAccelStruct(VkAccelerationStructureKHR, VObjectId, PVBuffer);
	~VulkanAccelStruct();

	PVBuffer m_buffer;
};

}
