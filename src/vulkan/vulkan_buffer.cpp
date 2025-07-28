// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, VMemoryBlock memory_block,
						   VBufferUsageFlags usage)
	: VulkanObjectBase(handle, id), m_memory_block(memory_block), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() {
	deferredRelease(vkDestroyBuffer, m_handle);
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
	Cleanup cleanup1([&]() { vkDestroyBuffer(device_handle, handle, nullptr); });

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device_handle, handle, &requirements);

	auto mem_block = EX_PASS(device.alloc(mem_usage, requirements));
	Cleanup cleanup2([&]() { device.memory().immediateFree(mem_block.id); });
	FWK_VK_EXPECT_CALL(vkBindBufferMemory, device_handle, handle, mem_block.handle,
					   mem_block.offset);
	mem_block.size = min<u32>(size, mem_block.size);

	cleanup1.cancel = true;
	cleanup2.cancel = true;
	return device.createObject(handle, mem_block, usage);
}

Ex<PVBuffer> VulkanBuffer::createAndUpload(VulkanDevice &device, CSpan<char> data,
										   VBufferUsageFlags usage, VMemoryUsage mem_usage) {
	auto out = EX_PASS(create(device, data.size(), usage, mem_usage));
	EXPECT(out->upload(data));
	return out;
}

Ex<> VulkanBuffer::upload(CSpan<char> data, uint byte_offset) {
	DASSERT_LE(byte_offset, size());
	if(!data)
		return {};

	// TODO: add option to invalidate previous contents?
	DASSERT_LE(u32(data.size()), size() - byte_offset);
	auto &device = this->device();
	auto &mem_mgr = device.memory();
	auto mem_block = m_memory_block;
	if(canBeMapped(mem_block.id.domain())) {
		mem_block.offset += byte_offset;
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size - byte_offset, data.size());
		fwk::copy(mem_mgr.writeAccessMemory(mem_block), data);
	} else {
		// TODO: minimize staging buffers allocations for many small uploads
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			device, data.size(), VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		fwk::copy(mem_mgr.writeAccessMemory(mem_block), data);
		auto &cmd_queue = device.cmdQueue();
		cmd_queue.copy({ref(), byte_offset}, staging_buffer);
	}

	return {};
}
}
