// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vulkan/vulkan_storage.h"
#include "fwk/vulkan_base.h"

#include "fwk/math/matrix4.h"

namespace fwk {

DEFINE_ENUM(VAccelStructType, top_level, bottom_level);

struct VAccelStructInstance {
	PVAccelStruct as;
	Matrix4 transform;
};

class VulkanAccelStruct : public VulkanObjectBase<VulkanAccelStruct> {
  public:
	static Ex<PVAccelStruct> create(VulkanDevice &, VAccelStructType, VBufferSpan<char>);

	static Ex<PVAccelStruct> buildBottom(VulkanDevice &, VBufferSpan<float3> vertices,
										 VBufferSpan<u32> indices);

	static Ex<PVAccelStruct> buildTop(VulkanDevice &, CSpan<VAccelStructInstance>);

	VkDeviceAddress deviceAddress() const;
	VAccelStructType type() const { return m_type; }

  private:
	friend class VulkanDevice;
	VulkanAccelStruct(VkAccelerationStructureKHR, VObjectId, PVBuffer, VAccelStructType);
	~VulkanAccelStruct();

	PVBuffer m_buffer;
	PVBuffer m_scratch_buffer;
	VAccelStructType m_type;
};

}
