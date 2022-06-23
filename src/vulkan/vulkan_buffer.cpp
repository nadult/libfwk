// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_buffer.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_ptr.h"
#include "vulkan/vulkan.h"

namespace fwk {

Ex<VPtr<VkBuffer>> VulkanBuffer::make(VDeviceId device_id, u64 size) {
	auto &vulkan = VulkanInstance::instance();
	auto device = vulkan[device_id].handle;

	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;
	ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer buffer_handle;

	if(vkCreateBuffer(device, &ci, nullptr, &buffer_handle) != VK_SUCCESS)
		return ERROR("Failed to create buffer");
	return PVBuffer::make(device_id, buffer_handle, size);
}

}