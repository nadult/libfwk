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
	static constexpr int max_colors = VulkanLimits::max_color_attachments;
	static constexpr int max_attachments = max_colors + 1;

	static PVFramebuffer create(PVRenderPass, CSpan<PVImageView>);
	static u32 hashConfig(CSpan<PVImageView>);

	PVRenderPass renderPass() const { return m_render_pass; }
	CSpan<PVImageView> attachments() const { return m_attachments; }
	CSpan<PVImageView> colors() const { return cspan(m_attachments.data(), m_num_colors); }
	PVImageView depth() const { return m_has_depth ? m_attachments.back() : PVImageView(); }
	bool hasDepth() const { return m_has_depth; }
	int2 size() const { return m_size; }

  private:
	friend class VulkanDevice;
	VulkanFramebuffer(VkFramebuffer, VObjectId);
	~VulkanFramebuffer();

	StaticVector<PVImageView, max_attachments> m_attachments;
	PVRenderPass m_render_pass;
	int2 m_size;
	int m_num_colors;
	bool m_has_depth;
};
}