// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_ptr.h"

namespace fwk {

class VulkanRenderPass {
  public:
	static Ex<PVRenderPass> create(VDeviceRef, CSpan<VkAttachmentDescription>,
								   CSpan<VkSubpassDescription>, CSpan<VkSubpassDependency>);

  private:
	uint m_attachment_count;
	uint m_subpass_count;
	uint m_dependency_count;
};

}