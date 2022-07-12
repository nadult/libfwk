// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {
VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, VMemoryBlock memory_block,
						   VBufferUsageFlags usage)
	: VulkanObjectBase(handle, id), m_memory_block(memory_block), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() { deferredHandleRelease<VkBuffer, vkDestroyBuffer>(); }

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size, VBufferUsageFlags usage,
								  VMemoryUsage mem_usage) {
	VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	ci.size = size;
	ci.usage = toVk(usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer handle;
	FWK_VK_EXPECT_CALL(vkCreateBuffer, device, &ci, nullptr, &handle);
	Cleanup cleanup([&]() { vkDestroyBuffer(device, handle, nullptr); });

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, handle, &requirements);

	auto mem_block = EX_PASS(device->alloc(mem_usage, requirements));
	FWK_VK_EXPECT_CALL(vkBindBufferMemory, device, handle, mem_block.handle, mem_block.offset);

	cleanup.cancel = true;
	return device->createObject(handle, mem_block, usage);
}
}