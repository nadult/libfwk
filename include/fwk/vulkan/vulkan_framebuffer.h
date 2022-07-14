// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/static_vector.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanFramebuffer : public VulkanObjectBase<VulkanFramebuffer> {
  public:
	static constexpr int max_color_atts = VulkanLimits::max_color_attachments;

	static PVFramebuffer create(VDeviceRef, PVRenderPass, CSpan<PVImageView> colors,
								PVImageView depth = none);
	static u32 hashConfig(CSpan<PVImageView> colors, PVImageView depth);

	// TODO: somehow mark Ptrs that they are non-null?
	CSpan<PVImageView> colors() const { return m_colors; }
	PVImageView depth() const { return m_depth; }
	bool hasDepth() const { return m_depth; }

	int2 extent() const { return m_extent; }

  private:
	friend class VulkanDevice;
	VulkanFramebuffer(VkFramebuffer, VObjectId);
	~VulkanFramebuffer();

	StaticVector<PVImageView, max_color_atts> m_colors;
	PVImageView m_depth;
	int2 m_extent;
};
}