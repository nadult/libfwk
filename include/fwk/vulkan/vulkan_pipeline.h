// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/static_vector.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
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
	const auto &descriptorSetLayouts() const { return m_dsls; }

  private:
	friend class VulkanDevice;
	VulkanPipelineLayout(VkPipelineLayout, VObjectId, vector<PVDescriptorSetLayout>);
	~VulkanPipelineLayout();

	vector<PVDescriptorSetLayout> m_dsls;
};

// TODO: consistent naming
// TODO: natvis
struct DescriptorBindingInfo {
	static constexpr int stages_bit_mask = 0x3fff;
	static_assert(count<VShaderStage> < 14);

	static constexpr int max_sets = 32;
	static constexpr int max_bindings = 1024 * 1024;

	DescriptorBindingInfo(VDescriptorType type, VShaderStageFlags stages, uint binding,
						  uint count = 1, uint set = 0)
		: value(u64(stages.bits) | (u64(type) << 14) | (u64(count) << 18) | (u64(binding) << 38) |
				(u64(set) << 58)) {
		PASSERT(set < max_sets);
		PASSERT(binding < max_bindings);
	}
	explicit DescriptorBindingInfo(u64 encoded_value) : value(encoded_value) {}
	DescriptorBindingInfo() : value(0) {}

	void clearSet() { value = value & ((1ull << 58) - 1); }

	uint set() const { return uint(value >> 58); }
	uint binding() const { return uint((value >> 38) & 0xfffffu); }
	uint count() const { return uint((value >> 18) & 0xfffffu); }
	VDescriptorType type() const { return VDescriptorType((value >> 14) & 0xf); }
	VShaderStageFlags stages() const { return VShaderStageFlags(value & stages_bit_mask); }

	bool operator<(const DescriptorBindingInfo &rhs) const { return value < rhs.value; }
	bool operator==(const DescriptorBindingInfo &rhs) const { return value == rhs.value; }

	static vector<DescriptorBindingInfo> merge(CSpan<DescriptorBindingInfo>,
											   CSpan<DescriptorBindingInfo>);
	static vector<CSpan<DescriptorBindingInfo>> divideSets(CSpan<DescriptorBindingInfo>);

	static u32 hashIgnoreSet(CSpan<DescriptorBindingInfo>, u32 seed = 123);

	u64 value = 0;
};

// TODO: consistent naming
struct DescriptorPoolSetup {
	EnumMap<VDescriptorType, uint> sizes;
	uint max_sets = 1;
};

// TODO: would it be better to turn it into managed class?
// TODO: consistent naming
struct DescriptorSet {
	DescriptorSet(PVPipelineLayout layout, uint layout_index, PVDescriptorPool pool,
				  VkDescriptorSet handle)
		: layout(layout), layout_index(layout_index), pool(pool), handle(handle) {}
	DescriptorSet() {}

	struct Assignment {
		VDescriptorType type;
		uint binding;
		Variant<Pair<PVSampler, PVImageView>, PVBuffer> data;
	};

	static constexpr int max_assignments = 16;
	void update(CSpan<Assignment>);

	PVPipelineLayout layout;
	uint layout_index = 0;

	PVDescriptorPool pool;
	VkDescriptorSet handle = nullptr;
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

class VulkanDescriptorPool : public VulkanObjectBase<VulkanDescriptorPool> {
  public:
	Ex<DescriptorSet> alloc(PVPipelineLayout, uint index);

  private:
	friend class VulkanDevice;
	VulkanDescriptorPool(VkDescriptorPool, VObjectId, uint max_sets);
	~VulkanDescriptorPool();

	uint m_num_sets = 0, m_max_sets;
};

class VulkanSampler : public VulkanObjectBase<VulkanSampler> {
  public:
	const auto &params() const { return m_params; }

  private:
	friend class VulkanDevice;
	VulkanSampler(VkSampler, VObjectId, const VSamplingParams &);
	~VulkanSampler();

	VSamplingParams m_params;
};

class VulkanPipeline : public VulkanObjectBase<VulkanPipeline> {
  public:
	static Ex<PVPipelineLayout> createLayout(VDeviceRef, vector<PVDescriptorSetLayout>);
	static Ex<PVPipeline> create(VDeviceRef, VPipelineSetup);

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