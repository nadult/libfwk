// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_framebuffer.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_pipeline.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

VulkanFramebuffer::VulkanFramebuffer(VkFramebuffer handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}

VulkanFramebuffer::~VulkanFramebuffer() { deferredRelease(vkDestroyFramebuffer, m_handle); }

PVFramebuffer VulkanFramebuffer::create(PVRenderPass render_pass, CSpan<PVImageView> colors,
										PVImageView depth) {
	DASSERT(colors.size() <= VulkanLimits::max_color_attachments);

	array<VkImageView, max_color_atts + 1> attachments;
	int num_attachments = colors.size();
	for(int i : intRange(colors))
		attachments[i] = colors[i];
	if(depth)
		attachments[num_attachments++] = depth;
	int2 size = colors ? colors[0]->size2D() : depth ? depth->size2D() : int2();

	VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	ci.renderPass = render_pass;
	ci.attachmentCount = num_attachments;
	ci.pAttachments = attachments.data();
	ci.width = uint(size.x);
	ci.height = uint(size.y);
	ci.flags = num_attachments == 0 ? VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT : 0;
	ci.layers = 1;

	auto &device = render_pass->device();
	VkFramebuffer handle;
	FWK_VK_CALL(vkCreateFramebuffer, device, &ci, nullptr, &handle);
	auto out = device.createObject(handle);
	out->m_colors = colors;
	out->m_depth = depth;
	out->m_size = size;
	return out;
}

u32 VulkanFramebuffer::hashConfig(CSpan<PVImageView> colors, PVImageView depth) {
	u32 out = 123;
	for(auto color : colors)
		out = combineHash(out, fwk::hash(u64(color.get())));
	out = combineHash(fwk::hash(u64(depth.get())), out);
	return out;
}
}
