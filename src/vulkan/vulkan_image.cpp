// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_storage.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanImage::VulkanImage(VkFormat format, int2 size)
	: m_format(format), m_size(size), m_is_external(false) {}
VulkanImage::~VulkanImage() {
	if(m_is_external)
		g_vk_storage.disableHandleDestruction<VkImage>(this);
}

Ex<PVImage> VulkanImage::create(VDeviceRef device, VkFormat format, int2 size) {
	return ERROR("write me please");
}

Ex<PVImage> VulkanImage::createExternal(VDeviceRef device, VkImage handle, VkFormat format,
										int2 size) {
	auto out = g_vk_storage.allocObject(device, handle, format, size);
	out->m_is_external = true;
	return out;
}

VulkanImageView::VulkanImageView(PVImage image, VkFormat format, int2 size)
	: m_image(image), m_size(size), m_format(format) {}
VulkanImageView ::~VulkanImageView() = default;

Ex<PVImageView> VulkanImageView::create(VDeviceRef device, PVImage image) {
	VkImageViewCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = image;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = image->format();
	ci.components.a = ci.components.b = ci.components.g = ci.components.r =
		VK_COMPONENT_SWIZZLE_IDENTITY;
	ci.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						   .baseMipLevel = 0,
						   .levelCount = 1,
						   .baseArrayLayer = 0,
						   .layerCount = 1};
	VkImageView handle;
	if(vkCreateImageView(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateImageView failed");
	return g_vk_storage.allocObject(device, handle, image, image->format(), image->size());
}
}