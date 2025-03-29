// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_render_pass.h"

#include "fwk/index_range.h"
#include "fwk/sys/assert.h"
#include "fwk/vulkan/vulkan_buffer_span.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

VulkanRenderPass::VulkanRenderPass(VkRenderPass handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}
VulkanRenderPass ::~VulkanRenderPass() {
	// TODO: here we probably don't need defer
	deferredRelease(vkDestroyRenderPass, m_handle);
}

u32 VulkanRenderPass::hashConfig(CSpan<VAttachment> attachments) { return fwk::hash(attachments); }

PVRenderPass VulkanRenderPass::create(VulkanDevice &device, CSpan<VAttachment> attachments) {
	array<VkAttachmentDescription, max_attachments> vk_attachments;
	array<VkAttachmentReference, max_attachments> vk_attachment_refs;

	for(int i = 0; i < attachments.size() - 1; i++)
		DASSERT(attachments[i].type() == VAttachmentType::color);

	const VAttachment *depth_attachment = nullptr;
	int num_colors = attachments.size();
	if(attachments && hasDepth(attachments.back().type())) {
		depth_attachment = &attachments.back();
		num_colors--;
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = num_colors;
	subpass.pColorAttachments = vk_attachment_refs.data();

	for(int i : intRange(num_colors)) {
		auto &src = attachments[i];
		auto &dst = vk_attachments[i];
		dst.flags = 0;
		dst.format = toVk(src.colorFormat());
		dst.samples = VkSampleCountFlagBits(src.numSamples());
		auto sync = src.sync();
		dst.loadOp = toVk(sync.loadOp());
		dst.storeOp = toVk(sync.storeOp());
		dst.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		dst.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		dst.initialLayout = toVk(sync.initialLayout());
		dst.finalLayout = toVk(sync.finalLayout());
		vk_attachment_refs[i].attachment = i;
		vk_attachment_refs[i].layout = toVk(sync.subpassLayout());
	}

	int depth_index = -1;
	if(depth_attachment) {
		depth_index = num_colors;
		auto &src = *depth_attachment;
		auto &dst = vk_attachments[depth_index];
		dst.flags = 0;
		dst.format = toVk(src.depthStencilFormat());
		dst.samples = VkSampleCountFlagBits(src.numSamples());
		auto sync = src.sync();
		dst.loadOp = toVk(sync.loadOp());
		dst.storeOp = toVk(sync.storeOp());
		dst.stencilLoadOp = toVk(sync.stencilLoadOp());
		dst.stencilStoreOp = toVk(sync.stencilStoreOp());
		dst.initialLayout = toVk(sync.initialLayout());
		dst.finalLayout = toVk(sync.finalLayout());
		vk_attachment_refs[depth_index].attachment = depth_index;
		vk_attachment_refs[depth_index].layout = toVk(sync.subpassLayout());
		subpass.pDepthStencilAttachment = vk_attachment_refs.data() + depth_index;
	}

	VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	ci.attachmentCount = attachments.size();
	ci.pAttachments = vk_attachments.data();
	ci.subpassCount = 1;
	ci.pSubpasses = &subpass;

	VkRenderPass handle;
	FWK_VK_CALL(vkCreateRenderPass, device.handle(), &ci, nullptr, &handle);
	auto out = device.createObject(handle);
	out->m_attachments = attachments;
	if(depth_attachment)
		out->m_depth_attachment = *depth_attachment;
	out->m_num_color_attachments = num_colors;
	return out;
}

auto VulkanRenderPass::computeAttachments(CSpan<PVImageView> image_views, VSimpleSync sync)
	-> AttachmentsVector {
	AttachmentsVector out;
	for(auto &image_view : image_views) {
		auto format = image_view->format();
		auto num_samples = image_view->dimensions().num_samples;
		format.visit([&](auto format) { out.emplace_back(format, sync, num_samples); });
	}
	return out;
}

auto VulkanRenderPass::computeAttachments(CSpan<PVImageView> image_views, CSpan<VSimpleSync> sync)
	-> AttachmentsVector {
	AttachmentsVector out;
	PASSERT(sync.size() == image_views.size());
	for(int i : intRange(image_views)) {
		auto format = image_views[i]->format();
		auto num_samples = image_views[i]->dimensions().num_samples;
		format.visit([&](auto format) { out.emplace_back(format, sync[i], num_samples); });
	}
	return out;
}

auto VulkanRenderPass::computeAttachments(CSpan<PVImageView> image_views,
										  CSpan<VAttachmentSync> syncs) -> AttachmentsVector {
	AttachmentsVector out;
	for(int i : intRange(image_views)) {
		auto format = image_views[i]->format();
		auto num_samples = image_views[i]->dimensions().num_samples;
		format.visit([&](auto format) { out.emplace_back(format, syncs[i], num_samples); });
	}
	return out;
}

}
