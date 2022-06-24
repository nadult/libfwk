// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanFramebuffer {
  public:
	VulkanFramebuffer(vector<PVImageView>, VkExtent2D);
	~VulkanFramebuffer();

	static Ex<PVFramebuffer> create(VDeviceRef, vector<PVImageView>, PVRenderPass);

	VkExtent2D extent() const { return m_extent; }

  private:
	vector<PVImageView> m_image_views;
	VkExtent2D m_extent;
};

}