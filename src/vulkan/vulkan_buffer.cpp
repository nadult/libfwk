// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_storage.h"
#include "vulkan/vulkan.h"

namespace fwk {
VulkanBuffer::VulkanBuffer(u64 size) : m_size(size) {}
VulkanBuffer::~VulkanBuffer() {
	// TODO: shouldn't we destroy buffer first ?
	// TODO: move memory to separate object
	if(m_memory)
		vkFreeMemory(m_device, m_memory, nullptr);
}

Ex<PVBuffer> VulkanBuffer::create(VDeviceRef device, u64 size) {
	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer buffer_handle;

	if(vkCreateBuffer(device->handle(), &ci, nullptr, &buffer_handle) != VK_SUCCESS)
		return ERROR("Failed to create buffer");
	auto out = g_vk_storage.allocObject<VkBuffer>(device, buffer_handle, size);

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