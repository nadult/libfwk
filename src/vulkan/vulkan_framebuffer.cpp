// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_framebuffer.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "vulkan/vulkan.h"

namespace fwk {

VulkanFramebuffer::VulkanFramebuffer(VkFramebuffer handle, VObjectId id,
									 vector<PVImageView> img_views, VkExtent2D extent)
	: VulkanObjectBase(handle, id), m_image_views(move(img_views)), m_extent(extent) {}

VulkanFramebuffer::~VulkanFramebuffer() {
	deferredHandleRelease<VkFramebuffer, vkDestroyFramebuffer>();
}

Ex<PVFramebuffer> VulkanFramebuffer::create(VDeviceRef device, vector<PVImageView> image_views,
											PVRenderPass render_pass) {
	DASSERT(!image_views.empty());

	auto attachments = transform(image_views, [](const auto &ptr) { return ptr.handle(); });
	auto extent = image_views[0]->extent();

	VkFramebufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	ci.renderPass = render_pass;
	ci.attachmentCount = attachments.size();
	ci.pAttachments = attachments.data();
	ci.width = extent.width;
	ci.height = extent.height;
	ci.layers = 1;

	VkFramebuffer handle;
	if(vkCreateFramebuffer(device->handle(), &ci, nullptr, &handle) != VK_SUCCESS)
		return ERROR("vkCreateFramebuffer failed");
	return device->createObject(handle, move(image_views), extent);
}
}