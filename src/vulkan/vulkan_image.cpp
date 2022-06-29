// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanImage::VulkanImage(VkImage handle, VObjectId id, VkExtent2D extent, const ImageSetup &setup)
	: VulkanObjectBase(handle, id), m_format(setup.format), m_extent(extent), m_usage(setup.usage),
	  m_num_samples(setup.num_samples), m_last_layout(setup.layout), m_is_external(false) {}

VulkanImage::~VulkanImage() {
	if(!m_is_external)
		deferredHandleRelease<VkImage, vkDestroyImage>();
}

Ex<PVImage> VulkanImage::create(VDeviceRef device, VkExtent2D extent, const ImageSetup &setup) {
	VkImageCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.extent.width = extent.width;
	ci.extent.height = extent.height;
	ci.extent.depth = 1;
	ci.format = setup.format;
	ci.arrayLayers = 1;
	ci.mipLevels = 1;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.initialLayout = toVk(setup.layout);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.usage = setup.usage.bits;

	DASSERT(setup.num_samples >= 1 && setup.num_samples < 64);
	DASSERT(isPowerOfTwo(setup.num_samples));
	ci.samples = VkSampleCountFlagBits(setup.num_samples);

	VkImage handle;
	if(vkCreateImage(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateImage failed");
	return device->createObject(handle, extent, setup);
}

Ex<PVImage> VulkanImage::createExternal(VDeviceRef device, VkImage handle, VkExtent2D extent,
										const ImageSetup &setup) {
	auto out = device->createObject(handle, extent, setup);
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

VkMemoryRequirements VulkanImage::memoryRequirements() const {
	VkMemoryRequirements mem_requirements;
	vkGetImageMemoryRequirements(deviceHandle(), m_handle, &mem_requirements);
	return mem_requirements;
}

Ex<void> VulkanImage::bindMemory(PVDeviceMemory memory) {
	if(vkBindImageMemory(deviceHandle(), m_handle, memory, 0) != VK_SUCCESS)
		return ERROR("vkBindImageMemory failed");
	m_memory = memory;
	m_memory_flags = m_memory->flags();
	return {};
}

}