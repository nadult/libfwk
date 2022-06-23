// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_render_pass.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_ptr.h"
#include "vulkan/vulkan.h"

namespace fwk {

Ex<PVRenderPass> VulkanRenderPass::create(VDeviceRef device,
										  CSpan<VkAttachmentDescription> attachments,
										  CSpan<VkSubpassDescription> subpasses,
										  CSpan<VkSubpassDependency> dependencies) {
	VkRenderPassCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = attachments.data();
	ci.subpassCount = subpasses.size();
	ci.pSubpasses = subpasses.data();
	ci.dependencyCount = dependencies.size();
	ci.pDependencies = dependencies.data();
	VkRenderPass handle;
	if(vkCreateRenderPass(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateRenderPass failed");
	auto out = PVRenderPass::create(device->id(), handle);
	out->m_attachment_count = attachments.size();
	out->m_subpass_count = subpasses.size();
	out->m_dependency_count = dependencies.size();
	return out;
}

}