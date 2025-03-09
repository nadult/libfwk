// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/gfx/image.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

static VkFormat toVk(VFormatVariant format) {
	return format.is<VColorFormat>() ? toVk(format.get<VColorFormat>()) :
									   toVk(format.get<VDepthStencilFormat>());
}

VImageDimensions::VImageDimensions(int3 size, uint num_mip_levels, uint num_samples)
	: size(size), num_mip_levels(num_mip_levels), num_samples(num_samples) {
	DASSERT(size.x >= 1 && size.y >= 1 && size.z >= 1);
	DASSERT(max(size.x, size.y, size.z) <= VulkanLimits::max_image_size);
	DASSERT(num_mip_levels >= 1 && num_mip_levels <= VulkanLimits::max_mip_levels);
}

VImageDimensions::VImageDimensions(int2 size, uint num_mip_levels, uint num_samples)
	: VImageDimensions(int3(size, 1), num_mip_levels, num_samples) {}

VImageSetup::VImageSetup(VColorFormat format, VImageDimensions dims, VImageUsageFlags usage,
						 VImageLayout layout)
	: dims(dims), format(format), usage(usage), layout(layout) {}

VImageSetup::VImageSetup(VDepthStencilFormat format, VImageDimensions dims, VImageUsageFlags usage,
						 VImageLayout layout)
	: dims(dims), format(format), usage(usage), layout(layout) {}

VImageSetup::VImageSetup(VFormatVariant format, VImageDimensions dims, VImageUsageFlags usage,
						 VImageLayout layout)
	: dims(dims), format(format), usage(usage), layout(layout) {}

VulkanImage::VulkanImage(VkImage handle, VObjectId id, VMemoryBlock mem_block,
						 const VImageSetup &setup)
	: VulkanObjectBase(handle, id), m_memory_block(mem_block), m_format(setup.format),
	  m_dims(setup.dims), m_usage(setup.usage), m_is_external(false) {
	for(int mip : intRange(setup.dims.num_mip_levels))
		setLayout(setup.layout, mip);
}

VulkanImage::~VulkanImage() {
	if(!m_is_external) {
		deferredRelease(vkDestroyImage, m_handle);
		if(m_memory_block.id.requiresFree())
			deferredFree(m_memory_block.id);
	}
}

Ex<PVImage> VulkanImage::create(VulkanDevice &device, const VImageSetup &setup,
								VMemoryUsage mem_usage) {
	VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.extent.width = uint(setup.dims.size.x);
	ci.extent.height = uint(setup.dims.size.y);
	ci.extent.depth = uint(setup.dims.size.z);
	ci.format = toVk(setup.format);
	ci.arrayLayers = 1;
	ci.mipLevels = setup.dims.num_mip_levels;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.initialLayout = toVk(setup.layout);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.usage = toVk(setup.usage);

	DASSERT(setup.dims.num_samples >= 1 &&
			setup.dims.num_samples <= VulkanLimits::max_image_samples);
	DASSERT(isPowerOfTwo(setup.dims.num_samples));
	ci.samples = VkSampleCountFlagBits(setup.dims.num_samples);

	VkImage handle;
	FWK_VK_EXPECT_CALL(vkCreateImage, device, &ci, nullptr, &handle);
	Cleanup cleanup([&]() { vkDestroyImage(device, handle, nullptr); });

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device, handle, &requirements);
	auto mem_block = EX_PASS(device.alloc(mem_usage, requirements));
	FWK_VK_EXPECT_CALL(vkBindImageMemory, device, handle, mem_block.handle, mem_block.offset);

	cleanup.cancel = true;
	return device.createObject(handle, mem_block, setup);
}

PVImage VulkanImage::createExternal(VulkanDevice &device, VkImage handle,
									const VImageSetup &setup) {
	auto out = device.createObject(handle, VMemoryBlock(), setup);
	out->m_is_external = true;
	return out;
}

Ex<PVImage> VulkanImage::createAndUpload(VulkanDevice &device, CSpan<Image> images) {
	DASSERT(images);
	for(auto image : images)
		DASSERT(image.format() == images[0].format());
	VImageSetup setup(images[0].format(), VImageDimensions(images[0].size(), images.size()));
	auto out = EX_PASS(create(device, setup));
	EXPECT(out->upload(images));
	return out;
}

int3 VulkanImage::mipSize(int mip_level) const {
	DASSERT(mip_level >= 0 && mip_level < m_dims.num_mip_levels);
	auto out = size();
	out.x = max(1, out.x >> mip_level);
	out.y = max(1, out.y >> mip_level);
	out.z = max(1, out.z >> mip_level);
	return out;
}

static constexpr uint layout_bits = 4;
static constexpr u64 layout_mask = ((1 << layout_bits) - 1);

VImageLayout VulkanImage::layout(int mip_level) const {
	static_assert(count<VImageLayout> <= (1 << layout_bits));
	return Layout((m_layout_bits >> (mip_level * layout_bits)) & layout_mask);
}

void VulkanImage::setLayout(Layout layout, int mip_level) {
	uint shift = mip_level * layout_bits;
	m_layout_bits &= ~(layout_mask << shift);
	m_layout_bits |= u64(layout) << shift;
}

// TODO: option to transition multiple mip levels at once
void VulkanImage::transitionLayout(VImageLayout new_layout, int mip_level) {
	auto old_layout = layout(mip_level);
	if(old_layout == new_layout)
		return;

	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = toVk(old_layout);
	barrier.newLayout = toVk(new_layout);
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_handle;
	barrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.baseMipLevel = uint(mip_level),
								.levelCount = 1,
								.layerCount = 1};
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags src_stage, dst_stage;
	// TODO: properly handle access masks & stages
	if(old_layout == VImageLayout::undefined &&
	   isOneOf(new_layout, VImageLayout::transfer_dst, VImageLayout::general)) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if(old_layout == VImageLayout::transfer_dst && new_layout == VImageLayout::shader_ro) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if((old_layout == VImageLayout::color_att && new_layout == VImageLayout::general) ||
			  (old_layout == VImageLayout::general && new_layout == VImageLayout::color_att)) {
		// TODO: too generic
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
		src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else {
		FWK_FATAL("Unsupported layout transition: %s -> %s", toString(old_layout),
				  toString(new_layout));
	}

	auto &cmds = device().cmdQueue();
	DASSERT(cmds.status() == VCommandQueueStatus::frame_running);
	auto cmd_buffer = cmds.bufferHandle();
	vkCmdPipelineBarrier(cmd_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	setLayout(new_layout, mip_level);
}

Ex<> VulkanImage::upload(CSpan<Image> src, VImageLayout dst_layout) {
	DASSERT(m_dims.num_mip_levels >= src.size());
	for(int mip : intRange(src))
		EXPECT(upload(src[mip], {0, 0}, mip, dst_layout));
	return {};
}

Ex<> VulkanImage::upload(const Image &src, int2 target_offset, int target_mip,
						 Layout target_layout) {
	if(src.empty())
		return {};
	DASSERT(target_mip >= 0 && target_mip < m_dims.num_mip_levels);
	DASSERT(src.format() == m_format);
	// TODO: dassert correct size

	int data_size = src.data().size();
	auto &device = this->device();
	auto &mem_mgr = device.memory();

	auto staging_buffer = EX_PASS(
		VulkanBuffer::create(device, data_size, VBufferUsage::transfer_src, VMemoryUsage::host));
	auto mem_block = staging_buffer->memoryBlock();
	fwk::copy(mem_mgr.writeAccessMemory(mem_block), src.data().reinterpret<char>());
	IBox box(int3{target_offset, 0}, int3{target_offset + src.size(), 1});
	device.cmdQueue().copy(ref(), staging_buffer, box, target_mip, target_layout);

	return {};
}

VulkanImageView::VulkanImageView(VkImageView handle, VObjectId id) : VulkanObjectBase(handle, id) {}
VulkanImageView ::~VulkanImageView() { deferredRelease(vkDestroyImageView, m_handle); }

PVImageView VulkanImageView::create(PVImage image) {
	VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	auto format = image->format();
	ci.image = image;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = toVk(format);
	ci.components.a = ci.components.b = ci.components.g = ci.components.r =
		VK_COMPONENT_SWIZZLE_IDENTITY;
	uint aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
	if(const VDepthStencilFormat *ds_format = format)
		aspect_mask =
			VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(*ds_format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	auto dims = image->dimensions();
	ci.subresourceRange = {.aspectMask = aspect_mask,
						   .baseMipLevel = 0,
						   .levelCount = dims.num_mip_levels,
						   .baseArrayLayer = 0,
						   .layerCount = 1};

	auto &device = image->device();
	VkImageView handle;
	FWK_VK_CALL(vkCreateImageView, device, &ci, nullptr, &handle);
	auto out = device.createObject(handle);
	out->m_image = image;
	out->m_format = image->format();
	out->m_dims = dims;
	return out;
}

}
