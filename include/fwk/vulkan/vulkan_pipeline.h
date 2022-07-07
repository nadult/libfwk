// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/static_vector.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VulkanLimits {
	static constexpr int max_color_attachments = 8;
	static constexpr int max_descr_sets = 32;
	static constexpr int max_descr_bindings = 1024 * 1024;
};

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

struct VBlendingMode {
	VBlendingMode(VBlendFactor src_color, VBlendFactor dst_color, VBlendOp color_op,
				  VBlendFactor src_alpha, VBlendFactor dst_alpha, VBlendOp alpha_op,
				  VColorComponents write_mask = all<VColorComponent>)
		: encoded_value(u32(src_color) | (u32(dst_color) << 5) | (u32(src_alpha) << 10) |
						(u32(dst_alpha) << 15) | (u32(color_op) << 20) | (u32(alpha_op) << 24) |
						(u32(write_mask.bits) << 28)) {}
	VBlendingMode(VColorComponents write_mask = all<VColorComponent>)
		: encoded_value(0xfffffffu | (u32(write_mask.bits) << 28)) {}
	VBlendingMode() : encoded_value(~0u) {}

	VColorComponents writeMask() const { return VColorComponents(encoded_value >> 28); }
	void setWriteMask(VColorComponents bits) {
		encoded_value = (encoded_value & 0xfffffffu) | (u32(bits.bits) << 28);
	}

	bool enabled() const { return (encoded_value & 0xfffffffu) == 0xfffffffu; }
	auto srcColor() const { return VBlendFactor((encoded_value >> 0) & 0x1f); }
	auto dstColor() const { return VBlendFactor((encoded_value >> 5) & 0x1f); }
	auto colorOp() const { return VBlendOp((encoded_value >> 20) & 0xf); }
	auto srcAlpha() const { return VBlendFactor((encoded_value >> 10) & 0x1f); }
	auto dstAlpha() const { return VBlendFactor((encoded_value >> 15) & 0x1f); }
	auto alphaOp() const { return VBlendOp((encoded_value >> 24) & 0xf); }

	u32 encoded_value;
};

struct VRasterSetup {
	VRasterSetup(VPrimitiveTopology topo = VPrimitiveTopology::triangle_list,
				 VPolygonMode poly = VPolygonMode::fill, VCullMode cull_mode = none,
				 VFrontFace front = VFrontFace::ccw, VRasterFlags flags = none,
				 float line_width = 1.0)
		: encoded_options(u32(topo) | (u32(poly) << 4) | (u32(cull_mode.bits) << 6) |
						  (u32(front) << 8) | (u32(flags.bits) << 10)),
		  line_width(line_width) {}

	auto primitiveTopology() const { return VPrimitiveTopology(encoded_options & 0xf); }
	auto polygonMode() const { return VPolygonMode((encoded_options >> 4) & 3); }
	auto cullMode() const { return VCullMode((encoded_options >> 6) & 3); }
	auto frontFace() const { return VFrontFace((encoded_options >> 8) & 3); }
	auto flags() const { return VRasterFlags((encoded_options >> 10) & 7); }

	u32 encoded_options;
	float line_width;
};

struct VDepthSetup {
	struct Bias {
		Bias(float constant_factor = 0.0f, float clamp = 0.0f, float slope_factor = 0.0f)
			: constant_factor(constant_factor), clamp(clamp), slope_factor(slope_factor) {}

		float constant_factor;
		float clamp;
		float slope_factor;
	};

	struct Bounds {
		Bounds(float min = 0.0f, float max = 1.0f) : min(min), max(max) {}
		float min, max;
	};

	VDepthSetup(VDepthFlags flags = none, VCompareOp compare_op = VCompareOp::less, Bias bias = {},
				Bounds bounds = {})
		: encoded_options(u32(flags.bits) | (u32(compare_op) << 8)), bias(bias), bounds(bounds) {}

	auto flags() const { return VDepthFlags(encoded_options & 0xff); }
	auto compareOp() const { return VCompareOp(encoded_options >> 8); }

	u32 encoded_options;
	Bias bias;
	Bounds bounds;
};

struct VStencilSetup {
	// TODO
};

struct VBlendingSetup {
	StaticVector<VBlendingMode, VulkanLimits::max_color_attachments> attachments;
	float4 constant = float4(0.0f);
};

struct VViewport {
	VViewport(IRect rect, float min_depth = 0.0, float max_depth = 1.0)
		: rect(rect), min_depth(min_depth), max_depth(max_depth) {}

	IRect rect;
	float min_depth, max_depth;
};

struct VPipelineSetup {
	StaticVector<PVShaderModule, count<VShaderStage>> shader_modules;
	PVRenderPass render_pass;

	vector<VertexBindingDesc> vertex_bindings;
	vector<VertexAttribDesc> vertex_attribs;

	VViewport viewport = IRect(0, 0, 1280, 720);
	Maybe<IRect> scissor;
	VRasterSetup raster;
	VDepthSetup depth;
	VStencilSetup stencil;
	VBlendingSetup blending;
};

struct AttachmentCore {
	AttachmentCore(VkFormat format, uint num_samples = 1)
		: format(format), num_samples(num_samples) {}
	FWK_TIE_MEMBERS(format, num_samples);
	FWK_TIED_COMPARES(AttachmentCore);

	VkFormat format;
	uint num_samples;
};

struct ColorAttachmentSync {
	ColorAttachmentSync(VLoadOp load, VStoreOp store, VLayout init_layout, VLayout final_layout)
		: load_op(load), store_op(store), init_layout(init_layout), final_layout(final_layout) {}
	FWK_TIE_MEMBERS(load_op, store_op, init_layout, final_layout);
	FWK_TIED_COMPARES(ColorAttachmentSync);

	VLoadOp load_op;
	VStoreOp store_op;
	VLayout init_layout;
	VLayout final_layout;
};

struct DepthAttachmentSync {
	DepthAttachmentSync(VLoadOp load, VStoreOp store, VLoadOp stencil_load, VStoreOp stencil_store,
						VLayout init_layout, VLayout final_layout)
		: load_op(load), store_op(store), stencil_load_op(stencil_load),
		  stencil_store_op(stencil_store), init_layout(init_layout), final_layout(final_layout) {}
	FWK_TIE_MEMBERS(load_op, store_op, stencil_load_op, stencil_store_op, init_layout,
					final_layout);
	FWK_TIED_COMPARES(DepthAttachmentSync);

	VLoadOp load_op;
	VStoreOp store_op;
	VLoadOp stencil_load_op;
	VStoreOp stencil_store_op;
	VLayout init_layout;
	VLayout final_layout;
};

struct RenderPassDesc {
	StaticVector<AttachmentCore, VulkanLimits::max_color_attachments> colors;
	StaticVector<ColorAttachmentSync, VulkanLimits::max_color_attachments> colors_sync;

	Maybe<AttachmentCore> depth;
	Maybe<DepthAttachmentSync> depth_sync;
};

class VulkanRenderPass : public VulkanObjectBase<VulkanRenderPass> {
  public:
	static Ex<PVRenderPass> create(VDeviceRef, const RenderPassDesc &);

	int numColorAttachments() const { return m_num_color_attachments; }

  private:
	friend class VulkanDevice;
	VulkanRenderPass(VkRenderPass, VObjectId, const RenderPassDesc &);
	~VulkanRenderPass();

	int m_num_color_attachments;
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
	static constexpr int max_sets = VulkanLimits::max_descr_sets,
						 max_bindings = VulkanLimits::max_descr_bindings;

	DescriptorBindingInfo(VDescriptorType type, VShaderStages stages, uint binding, uint count = 1,
						  uint set = 0)
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
	VShaderStages stages() const { return VShaderStages(value & stages_bit_mask); }

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