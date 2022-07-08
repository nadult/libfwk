// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_framebuffer.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanFramebuffer::VulkanFramebuffer(VkFramebuffer handle, VObjectId id)
	: VulkanObjectBase(handle, id) {}

VulkanFramebuffer::~VulkanFramebuffer() {
	deferredHandleRelease<VkFramebuffer, vkDestroyFramebuffer>();
}

Ex<PVFramebuffer> VulkanFramebuffer::create(VDeviceRef device, vector<PVImageView> image_views,
											PVRenderPass render_pass) {
	DASSERT(!image_views.empty());

	auto attachments = transform(image_views, [](const auto &ptr) { return ptr.handle(); });
	auto extent = image_views[0]->extent();

	VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	ci.renderPass = render_pass;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = attachments.data();
	ci.width = uint(extent.x);
	ci.height = uint(extent.y);
	ci.layers = 1;

	VkFramebuffer handle;
	if(vkCreateFramebuffer(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateFramebuffer failed");
	auto out = device->createObject(handle);
	out->m_image_views = move(image_views);
	out->m_extent = extent;
	return out;
}
}