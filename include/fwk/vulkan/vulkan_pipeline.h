// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/static_vector.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

DEFINE_ENUM(VShaderStage, vertex, fragment, compute);
DEFINE_ENUM(VTopology, point_list, line_list, line_strip, triangle_list, triangle_strip,
			triangle_fan);

struct VShaderStageSetup {
	PVShaderModule shader_module;
	VShaderStage stage;
};

struct VInputAssemblySetup {
	VTopology topology = VTopology::triangle_list;
	bool primitive_restart = false;
};

struct VPipelineSetup {
	StaticVector<VShaderStageSetup, count<VShaderStage>> stages;
	VInputAssemblySetup input_assembly;
	VkViewport viewport;
	VkRect2D scissor;
	PVRenderPass render_pass;
	// TODO: other stuff
};

class VulkanRenderPass {
  public:
	static Ex<PVRenderPass> create(VDeviceRef, CSpan<VkAttachmentDescription>,
								   CSpan<VkSubpassDescription>, CSpan<VkSubpassDependency>);

  private:
	uint m_attachment_count;
	uint m_subpass_count;
	uint m_dependency_count;
};

class VulkanPipeline {
  public:
	VulkanPipeline(PVRenderPass, PVPipelineLayout);
	~VulkanPipeline();

	static Ex<PVPipelineLayout> createLayout(VDeviceRef);
	static Ex<PVPipeline> create(VDeviceRef, const VPipelineSetup &);

	PVRenderPass renderPass() const { return m_render_pass; }
	PVPipelineLayout pipelineLayout() const { return m_pipeline_layout; }

  private:
	PVRenderPass m_render_pass;
	PVPipelineLayout m_pipeline_layout;
};

}