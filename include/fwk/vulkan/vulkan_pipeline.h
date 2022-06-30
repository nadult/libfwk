// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/static_vector.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

// TODO: make useful constructors
struct VertexAttribDesc {
	VkFormat format = VkFormat::VK_FORMAT_R32_SFLOAT;
	u16 offset = 0;
	u8 location_index = 0;
	u8 binding_index = 0;
};

struct VertexBindingDesc {
	u8 index = 0;
	VertexInputRate input_rate = VertexInputRate::vertex;
	u16 stride = 0;
};

struct VShaderStageSetup {
	PVShaderModule shader_module;
	VShaderStage stage;
};

struct VInputAssemblySetup {
	VPrimitiveTopology topology = VPrimitiveTopology::triangle_list;
	bool primitive_restart = false;
};

struct VPipelineSetup {
	StaticVector<VShaderStageSetup, count<VShaderStage>> stages;
	vector<PVDescriptorSetLayout> dsls;
	VInputAssemblySetup input_assembly;
	VkViewport viewport;
	VkRect2D scissor;
	PVRenderPass render_pass;
	vector<VertexBindingDesc> vertex_bindings;
	vector<VertexAttribDesc> vertex_attribs;
	// TODO: other stuff
};

class VulkanRenderPass : public VulkanObjectBase<VulkanRenderPass> {
  public:
	static Ex<PVRenderPass> create(VDeviceRef, CSpan<VkAttachmentDescription>,
								   CSpan<VkSubpassDescription>, CSpan<VkSubpassDependency>);

  private:
	friend class VulkanDevice;
	VulkanRenderPass(VkRenderPass, VObjectId);
	~VulkanRenderPass();

	uint m_attachment_count;
	uint m_subpass_count;
	uint m_dependency_count;
};

class VulkanPipelineLayout : public VulkanObjectBase<VulkanPipelineLayout> {
  public:
  private:
	friend class VulkanDevice;
	VulkanPipelineLayout(VkPipelineLayout, VObjectId);
	~VulkanPipelineLayout();
};

struct DescriptorBindingInfo {
	uint index = 0;
	uint count = 1;
	VShaderStageFlags stages = VShaderStage::vertex | VShaderStage::fragment;
	VDescriptorType type = VDescriptorType::sampler;
};

class VulkanDescriptorSetLayout : public VulkanObjectBase<VulkanDescriptorSetLayout> {
  public:
	using BindingInfo = DescriptorBindingInfo;

	CSpan<BindingInfo> bindings() const { return m_bindings; }

  private:
	friend class VulkanDevice;
	VulkanDescriptorSetLayout(VkDescriptorSetLayout, VObjectId, vector<BindingInfo>);
	~VulkanDescriptorSetLayout();

	vector<BindingInfo> m_bindings;
};

class VulkanPipeline : public VulkanObjectBase<VulkanPipeline> {
  public:
	static Ex<PVDescriptorSetLayout> createDescriptorSetLayout(VDeviceRef,
															   vector<DescriptorBindingInfo>);
	static Ex<PVPipelineLayout> createLayout(VDeviceRef, CSpan<PVDescriptorSetLayout>);
	static Ex<PVPipeline> create(VDeviceRef, const VPipelineSetup &);

	PVRenderPass renderPass() const { return m_render_pass; }
	PVPipelineLayout pipelineLayout() const { return m_pipeline_layout; }

  private:
	friend class VulkanDevice;
	VulkanPipeline(VkPipeline, VObjectId, PVRenderPass, PVPipelineLayout);
	~VulkanPipeline();

	PVRenderPass m_render_pass;
	PVPipelineLayout m_pipeline_layout;
};

}