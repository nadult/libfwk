// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_storage.h"

#include <vulkan/vulkan.h>

namespace fwk {
VulkanBuffer::VulkanBuffer(VkBuffer handle, VObjectId id, u64 size, VBufferUsage usage)
	: VulkanObjectBase(handle, id), m_size(size), m_usage(usage) {}

VulkanBuffer::~VulkanBuffer() {
	deferredHandleRelease(
		[](void *handle, VkDevice device) { vkDestroyBuffer(device, (VkBuffer)handle, nullptr); });
}

static const EnumMap<VBufferUsageFlag, VkBufferUsageFlagBits> usage_flags = {{
	{VBufferUsageFlag::transfer_src, VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
	{VBufferUsageFlag::transfer_dst, VK_BUFFER_USAGE_TRANSFER_DST_BIT},
	{VBufferUsageFlag::uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
	{VBufferUsageFlag::storage_buffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
	{VBufferUsageFlag::vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
	{VBufferUsageFlag::index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
	{VBufferUsageFlag::indirect_buffer, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT},
}};

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size, VBufferUsage usage) {
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = translateFlags(usage, usage_flags);
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
	return {};
}

void VulkanBuffer::upload(CSpan<char> bytes) {
	DASSERT(m_memory);

	void *device_data;
	auto device_handle = deviceHandle();
	vkMapMemory(device_handle, m_memory->handle(), 0, m_size, 0, &device_data);
	memcpy(device_data, bytes.data(), bytes.size());
	vkUnmapMemory(device_handle, m_memory->handle());
}

}