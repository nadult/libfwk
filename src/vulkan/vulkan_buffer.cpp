// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_device.h"

#include <vulkan/vulkan.h>

namespace fwk {
VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, VMemoryBlock memory_block,
						   VBufferUsageFlags usage)
	: VulkanObjectBase(handle, id), m_memory_block(memory_block), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() { deferredHandleRelease<VkBuffer, vkDestroyBuffer>(); }

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size, VBufferUsageFlags usage,
								  VMemoryUsage mem_usage) {
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = toVk(usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer handle;
	if(vkCreateBuffer(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("Failed to create buffer");

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device, handle, &requirements);

	auto mem_block = device->alloc(mem_usage, requirements);
	if(!mem_block) {
		vkDestroyBuffer(device, handle, nullptr);
		return mem_block.error();
	}
	if(vkBindBufferMemory(device, handle, mem_block->handle, mem_block->offset) != VK_SUCCESS) {
		vkDestroyBuffer(device, handle, nullptr);
		return ERROR("vkBindBufferMemory failed");
	}

	return device->createObject(handle, *mem_block, usage);
}
}