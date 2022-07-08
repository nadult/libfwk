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

	CSpan<PVImageView> imageViews() const { return m_image_views; }
	int2 extent() const { return m_extent; }

  private:
	friend class VulkanDevice;
	VulkanFramebuffer(VkFramebuffer, VObjectId);
	~VulkanFramebuffer();

	vector<PVImageView> m_image_views;
	int2 m_extent;
};

}