// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_device.h"

#include <vulkan/vulkan.h>

namespace fwk {
VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, u64 size, VBufferUsageFlags usage)
	: VulkanObjectBase(handle, id), m_size(size), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() { deferredHandleRelease<VkBuffer, vkDestroyBuffer>(); }

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size, VBufferUsageFlags usage) {
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = toVk(usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer buffer_handle;

	if(vkCreateBuffer(device->handle(), &ci, nullptr, &buffer_handle) != VK_SUCCESS)
		return ERROR("Failed to create buffer");
	return device->createObject(buffer_handle, size, usage);
}

VkMemoryRequirements VulkanBuffer::memoryRequirements() const {
	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(deviceHandle(), m_handle, &mem_requirements);
	return mem_requirements;
}

Ex<void> VulkanBuffer::bindMemory(PVDeviceMemory memory) {
	if(vkBindBufferMemory(deviceHandle(), m_handle, memory->handle(), 0) != VK_SUCCESS)
		return ERROR("vkBindBufferMemory failed");
	m_memory = memory;
	m_mem_flags = m_memory->flags();
	return {};
}

void VulkanBuffer::upload(CSpan<char> bytes) {
	DASSERT(m_memory);

	void *device_data;
	auto device_handle = deviceHandle();
	vkMapMemory(device_handle, m_memory->handle(), 0, m_size, 0, &device_data);
	memcpy(device_data, bytes.data(), bytes.size());
	VkMappedMemoryRange mem_range{};
	mem_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mem_range.memory = m_memory;
	mem_range.offset = 0;
	mem_range.size = bytes.size();
	vkFlushMappedMemoryRanges(device_handle, 1, &mem_range);
	vkUnmapMemory(device_handle, m_memory->handle());
}

}