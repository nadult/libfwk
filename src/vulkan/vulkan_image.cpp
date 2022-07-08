// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanImage::VulkanImage(VkImage handle, VObjectId id, VMemoryBlock mem_block,
						 const VImageSetup &setup)
	: VulkanObjectBase(handle, id), m_memory_block(mem_block), m_format(setup.format),
	  m_extent(setup.extent), m_usage(setup.usage), m_num_samples(setup.num_samples),
	  m_last_layout(setup.layout), m_is_external(false) {}

VulkanImage::~VulkanImage() {
	if(!m_is_external)
		deferredHandleRelease<VkImage, vkDestroyImage>();
}

Ex<PVImage> VulkanImage::create(VDeviceRef device, const VImageSetup &setup,
								VMemoryUsage mem_usage) {
	VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.extent.width = uint(setup.extent.x);
	ci.extent.height = uint(setup.extent.y);
	ci.extent.depth = 1;
	ci.format = setup.format;
	ci.arrayLayers = 1;
	ci.mipLevels = 1;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.initialLayout = toVk(setup.layout);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.usage = toVk(setup.usage);

	DASSERT(setup.num_samples >= 1 && setup.num_samples < 64);
	DASSERT(isPowerOfTwo(setup.num_samples));
	ci.samples = VkSampleCountFlagBits(setup.num_samples);

	VkImage handle;
	if(vkCreateImage(device, &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateImage failed");

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, handle, &requirements);

	auto mem_block = device->alloc(mem_usage, requirements);
	if(!mem_block) {
		vkDestroyImage(device, handle, nullptr);
		return mem_block.error();
	}

	if(vkBindImageMemory(device, handle, mem_block->handle, mem_block->offset) != VK_SUCCESS) {
		vkDestroyImage(device, handle, nullptr);
		return ERROR("vkBindImageMemory failed");
	}

	return device->createObject(handle, *mem_block, setup);
}

Ex<PVImage> VulkanImage::createExternal(VDeviceRef device, VkImage handle,
										const VImageSetup &setup) {
	auto out = device->createObject(handle, VMemoryBlock(), setup);
	out->m_is_external = true;
	return out;
}

VulkanImageView::VulkanImageView(VkImageView handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanImageView ::~VulkanImageView() { deferredHandleRelease<VkImageView, vkDestroyImageView>(); }

Ex<PVImageView> VulkanImageView::create(VDeviceRef device, PVImage image) {
	VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
	auto out = device->createObject(handle);
	out->m_image = image;
	out->m_format = image->format();
	out->m_extent = image->extent();
	out->m_num_samples = image->numSamples();
	return out;
}
}