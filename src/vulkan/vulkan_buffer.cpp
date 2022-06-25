// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_storage.h"

#include <vulkan/vulkan.h>

namespace fwk {
VulkanBuffer::VulkanBuffer(u64 size, VBufferUsage usage) : m_size(size), m_usage(usage) {}
VulkanBuffer::~VulkanBuffer() {
	// TODO: shouldn't we destroy buffer first ?
	// TODO: move memory to separate object
	if(m_memory)
		vkFreeMemory(m_device, m_memory, nullptr);
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

template <class Enum, class Bit>
VkFlags translateFlags(EnumFlags<Enum> flags, const EnumMap<Enum, Bit> &bit_map) {
	VkFlags out = 0;
	for(auto flag : flags)
		out |= bit_map[flag];
	return out;
}

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size, VBufferUsage usage) {
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = translateFlags(usage, usage_flags);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer buffer_handle;

	if(vkCreateBuffer(device->handle(), &ci, nullptr, &buffer_handle) != VK_SUCCESS)
		return ERROR("Failed to create buffer");
	auto out = g_vk_storage.allocObject<VkBuffer>(device, buffer_handle, size, usage);

	VkMemoryRequirements mem_requirements;
	vkGetBufferMemoryRequirements(device->handle(), out, &mem_requirements);
	auto &phys_info = VulkanInstance::ref()->info(device->physId());
	VkMemoryPropertyFlags flags =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	auto mem_type = phys_info.findMemoryType(mem_requirements.memoryTypeBits, flags);
	EXPECT(mem_type && "Couldn't find a suitable memory type");

	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = mem_requirements.size;
	ai.memoryTypeIndex = *mem_type;
	out->m_device = device->handle();
	if(vkAllocateMemory(device->handle(), &ai, nullptr, &out->m_memory) != VK_SUCCESS)
		return ERROR("vkAllocateMemory failed");
	vkBindBufferMemory(device->handle(), out, out->m_memory, 0);

	return out;
}

void VulkanBuffer::upload(CSpan<char> bytes) {
	// TODO: we need at least device_id here...
	void *device_data;
	vkMapMemory(m_device, m_memory, 0, m_size, 0, &device_data);
	memcpy(device_data, bytes.data(), bytes.size());
	vkUnmapMemory(m_device, m_memory);
}

}