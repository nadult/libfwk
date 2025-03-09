// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_framebuffer.h"

#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_render_pass.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

VulkanFramebuffer::VulkanFramebuffer(VkFramebuffer handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}

VulkanFramebuffer::~VulkanFramebuffer() { deferredRelease(vkDestroyFramebuffer, m_handle); }

PVFramebuffer VulkanFramebuffer::create(PVRenderPass render_pass, CSpan<PVImageView> attachments) {
	DASSERT(render_pass);
	DASSERT(attachments.size() <= max_attachments);
	for(int i = 0; i < attachments.size() - 1; i++)
		DASSERT(attachments[i]->format().is<VColorFormat>());

	int2 size = attachments ? attachments[0]->size2D() : int2();
	int num_colors = attachments.size();
	if(attachments && attachments.back()->format().is<VDepthStencilFormat>())
		num_colors--;

	array<VkImageView, max_attachments> vk_attachments;
	for(int i : intRange(attachments))
		vk_attachments[i] = attachments[i];

	VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	ci.renderPass = render_pass;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = vk_attachments.data();
	ci.width = uint(size.x);
	ci.height = uint(size.y);
	ci.flags = attachments ? 0 : VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
	ci.layers = 1;

	auto &device = render_pass->device();
	VkFramebuffer handle;
	FWK_VK_CALL(vkCreateFramebuffer, device, &ci, nullptr, &handle);
	auto out = device.createObject(handle);
	out->m_attachments = attachments;
	out->m_has_depth = num_colors < attachments.size();
	out->m_size = size;
	out->m_render_pass = render_pass;
	out->m_num_colors = num_colors;
	return out;
}

u32 VulkanFramebuffer::hashConfig(CSpan<PVImageView> attachments) {
	u32 out = 123;
	for(auto attachment : attachments)
		out = combineHash(out, fwk::hash(u64(attachment.get())));
	return out;
}
}
