// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/static_vector.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanRenderPass : public VulkanObjectBase<VulkanRenderPass> {
  public:
	static constexpr int max_attachments = VulkanLimits::max_color_attachments + 1;
	using AttachmentsVector = StaticVector<VAttachment, max_attachments>;

	// It's better not to use this function directly, use VulkanDevice::getRenderPass instead
	static PVRenderPass create(VulkanDevice &, CSpan<VAttachment>);
	static AttachmentsVector computeAttachments(CSpan<PVImageView>, VSimpleSync);
	static AttachmentsVector computeAttachments(CSpan<PVImageView>, CSpan<VAttachmentSync>);
	static u32 hashConfig(CSpan<VAttachment>);

	CSpan<VAttachment> attachments() const { return m_attachments; }
	CSpan<VAttachment> colors() const { return {m_attachments.data(), m_num_color_attachments}; }
	Maybe<VAttachment> depth() const { return m_depth_attachment; }
	u32 hash() const { return m_hash; }

  private:
	friend class VulkanDevice;
	VulkanRenderPass(VkRenderPass, VObjectId);
	~VulkanRenderPass();

	AttachmentsVector m_attachments;
	Maybe<VAttachment> m_depth_attachment;
	int m_num_color_attachments;
	u32 m_hash;
};

}