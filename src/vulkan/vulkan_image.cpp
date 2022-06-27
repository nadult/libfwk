// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanImage::VulkanImage(VkImage handle, VObjectId id, VkFormat format, VkExtent2D extent)
	: VulkanObjectBase(handle, id), m_format(format), m_extent(extent), m_is_external(false) {}
VulkanImage::~VulkanImage() {
	if(!m_is_external)
		deferredHandleRelease<VkImage, vkDestroyImage>();
}

Ex<PVImage> VulkanImage::create(VDeviceRef device, VkFormat format, VkExtent2D extent) {
	return ERROR("write me please");
}

Ex<PVImage> VulkanImage::createExternal(VDeviceRef device, VkImage handle, VkFormat format,
										VkExtent2D extent) {
	auto out = device->createObject(handle, format, extent);
	out->m_is_external = true;
	return out;
}

VulkanImageView::VulkanImageView(VkImageView handle, VObjectId id, PVImage image, VkFormat format,
								 VkExtent2D extent)
	: VulkanObjectBase(handle, id), m_image(image), m_extent(extent), m_format(format) {}
VulkanImageView ::~VulkanImageView() { deferredHandleRelease<VkImageView, vkDestroyImageView>(); }

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
	return device->createObject(handle, image, image->format(), image->extent());
}
}