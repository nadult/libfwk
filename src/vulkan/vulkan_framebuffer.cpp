// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_framebuffer.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

VulkanFramebuffer::VulkanFramebuffer(VkFramebuffer handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}

VulkanFramebuffer::~VulkanFramebuffer() { deferredRelease<vkDestroyFramebuffer>(m_handle); }

PVFramebuffer VulkanFramebuffer::create(VDeviceRef device, CSpan<PVImageView> colors,
										PVImageView depth, PVRenderPass render_pass) {
	DASSERT(!colors.empty());
	DASSERT(colors.size() <= VulkanLimits::max_color_attachments);

	array<VkImageView, max_color_atts + 1> attachments;
	int num_attachments = colors.size();
	for(int i : intRange(colors))
		attachments[i] = colors[i];
	if(depth)
		attachments[num_attachments++] = depth;
	auto extent = colors[0]->extent();

	if(!render_pass) {
		StaticVector<VColorAttachment, max_color_atts> color_atts;
		for(auto color : colors)
			color_atts.emplace_back(color->format(), color->numSamples());
		Maybe<VDepthAttachment> depth_att;
		if(depth)
			depth_att = VDepthAttachment(depth->format(), depth->numSamples());
		render_pass = device->getRenderPass(color_atts, depth_att);
	}

	VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	ci.renderPass = render_pass;
	ci.attachmentCount = num_attachments;
	ci.pAttachments = attachments.data();
	ci.width = uint(extent.x);
	ci.height = uint(extent.y);
	ci.layers = 1;

	VkFramebuffer handle;
	FWK_VK_CALL(vkCreateFramebuffer, device->handle(), &ci, nullptr, &handle);
	auto out = device->createObject(handle);
	out->m_colors = colors;
	out->m_depth = depth;
	out->m_extent = extent;
	return out;
}
}