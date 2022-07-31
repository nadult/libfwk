// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, VMemoryBlock memory_block,
						   VBufferUsageFlags usage)
	: VulkanObjectBase(handle, id), m_memory_block(memory_block), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() {
	deferredRelease<vkDestroyBuffer>(m_handle);
	if(m_memory_block.id.requiresFree())
		deferredFree(m_memory_block.id);
}

Ex<PVBuffer> VulkanBuffer::create(VulkanDevice &device, u64 size, VBufferUsageFlags usage,
								  VMemoryUsage mem_usage) {
	VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	ci.size = size;
	ci.usage = toVk(usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer handle;
	auto device_handle = device.handle();
	FWK_VK_EXPECT_CALL(vkCreateBuffer, device_handle, &ci, nullptr, &handle);
	Cleanup cleanup([&]() { vkDestroyBuffer(device_handle, handle, nullptr); });

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device_handle, handle, &requirements);

	auto mem_block = EX_PASS(device.alloc(mem_usage, requirements));
	FWK_VK_EXPECT_CALL(vkBindBufferMemory, device_handle, handle, mem_block.handle,
					   mem_block.offset);
	mem_block.size = min<u32>(size, mem_block.size);

	cleanup.cancel = true;
	return device.createObject(handle, mem_block, usage);
}

Ex<PVBuffer> VulkanBuffer::createAndUpload(VulkanDevice &device, CSpan<char> data,
										   VBufferUsageFlags usage, VMemoryUsage mem_usage) {
	auto out = EX_PASS(create(device, data.size(), usage, mem_usage));
	EXPECT(device.cmdQueue().upload(out, data));
	return out;
}
}