// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

VImageSetup::VImageSetup(VDepthStencilFormat ds_format, int2 extent, uint num_samples)
	: VImageSetup(toVk(ds_format), extent, num_samples, VImageUsage::depth_stencil_att,
				  VImageLayout::undefined) {}

VulkanImage::VulkanImage(VkImage handle, VObjectId id, VMemoryBlock mem_block,
						 const VImageSetup &setup)
	: VulkanObjectBase(handle, id), m_memory_block(mem_block), m_format(setup.format),
	  m_extent(setup.extent), m_usage(setup.usage), m_num_samples(setup.num_samples),
	  m_last_layout(setup.layout), m_is_external(false) {}

VulkanImage::~VulkanImage() {
	if(!m_is_external) {
		deferredRelease<vkDestroyImage>(m_handle);
		if(m_memory_block.id.requiresFree())
			deferredFree(m_memory_block.id);
	}
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
	FWK_VK_EXPECT_CALL(vkCreateImage, device, &ci, nullptr, &handle);
	Cleanup cleanup([&]() { vkDestroyImage(device, handle, nullptr); });

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, handle, &requirements);
	auto mem_block = EX_PASS(device->alloc(mem_usage, requirements));
	FWK_VK_EXPECT_CALL(vkBindImageMemory, device, handle, mem_block.handle, mem_block.offset);

	cleanup.cancel = true;
	return device->createObject(handle, mem_block, setup);
}

PVImage VulkanImage::createExternal(VDeviceRef device, VkImage handle, const VImageSetup &setup) {
	auto out = device->createObject(handle, VMemoryBlock(), setup);
	out->m_is_external = true;
	return out;
}

Maybe<VDepthStencilFormat> VulkanImage::depthStencilFormat() const {
	return fromVkDepthStencilFormat(m_format);
}

VulkanImageView::VulkanImageView(VkImageView handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanImageView ::~VulkanImageView() { deferredRelease<vkDestroyImageView>(m_handle); }

PVImageView VulkanImageView::create(VDeviceRef device, PVImage image) {
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
	FWK_VK_CALL(vkCreateImageView, device->handle(), &ci, nullptr, &handle);
	auto out = device->createObject(handle);
	out->m_image = image;
	out->m_format = image->format();
	out->m_extent = image->extent();
	out->m_num_samples = image->numSamples();
	return out;
}
}