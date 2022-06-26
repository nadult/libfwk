// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanFramebuffer : public VulkanObjectBase<VulkanFramebuffer> {
  public:
	static Ex<PVFramebuffer> create(VDeviceRef, vector<PVImageView>, PVRenderPass);

	VkExtent2D extent() const { return m_extent; }

  private:
	friend class VulkanDevice;
	VulkanFramebuffer(VkFramebuffer, VObjectId, vector<PVImageView>, VkExtent2D);
	~VulkanFramebuffer();

	vector<PVImageView> m_image_views;
	VkExtent2D m_extent;
};

}