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

template <class T> struct VAutoVulkanFormat {};
#define AUTO_FORMAT(type_, format_)                                                                \
	template <> struct VAutoVulkanFormat<type_> { static constexpr auto format = format_; };
AUTO_FORMAT(float, VK_FORMAT_R32_SFLOAT)
AUTO_FORMAT(float2, VK_FORMAT_R32G32_SFLOAT)
AUTO_FORMAT(float3, VK_FORMAT_R32G32B32_SFLOAT)
AUTO_FORMAT(float4, VK_FORMAT_R32G32B32A32_SFLOAT)
AUTO_FORMAT(int, VK_FORMAT_R32_SINT)
AUTO_FORMAT(int2, VK_FORMAT_R32G32_SINT)
AUTO_FORMAT(int3, VK_FORMAT_R32G32B32_SINT)
AUTO_FORMAT(int4, VK_FORMAT_R32G32B32A32_SINT)
AUTO_FORMAT(IColor, VK_FORMAT_R8G8B8A8_UNORM)
#undef AUTO_FORMAT

struct VVertexAttrib {
	VVertexAttrib(uint location_index, uint binding_index, uint offset = 0,
				  VkFormat format = VK_FORMAT_R32_SFLOAT)
		: format(format), offset(offset), location_index(location_index),
		  binding_index(binding_index) {}

	VkFormat format;
	u16 offset;
	u8 location_index;
	u8 binding_index;
};

template <class T> auto vertexAttrib(uint location_index, uint binding_index, uint offset = 0) {
	return VVertexAttrib(location_index, binding_index, offset, VAutoVulkanFormat<T>::format);
}

struct VVertexBinding {
	VVertexBinding(uint index, uint stride, VertexInputRate input_rate = VertexInputRate::vertex)
		: index(index), input_rate(input_rate), stride(stride) {}

	u8 index;
	VertexInputRate input_rate;
	u16 stride;
};

template <class T>
auto vertexBinding(uint index, VertexInputRate input_rate = VertexInputRate::vertex) {
	return VVertexBinding(index, sizeof(T), input_rate);
}

struct VBlendingMode {
	VBlendingMode(VBlendFactor src, VBlendFactor dst, VBlendOp op = VBlendOp::add,
				  VColorComponents write_mask = all<VColorComponent>)
		: VBlendingMode(src, dst, op, src, dst, op, write_mask) {}
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

	bool enabled() const { return (encoded_value & 0xfffffffu) != 0xfffffffu; }
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

	vector<VVertexBinding> vertex_bindings;
	vector<VVertexAttrib> vertex_attribs;

	VViewport viewport = IRect(0, 0, 1280, 720);
	Maybe<IRect> scissor;
	VRasterSetup raster;
	VDepthSetup depth;
	VStencilSetup stencil;
	VBlendingSetup blending;
	VDynamicState dynamic_state = none;
};

class VulkanRenderPass : public VulkanObjectBase<VulkanRenderPass> {
  public:
	static constexpr int max_colors = VulkanLimits::max_color_attachments;

	static Ex<PVRenderPass> create(VDeviceRef, CSpan<VColorAttachment>,
								   Maybe<VDepthAttachment> = none);
	static u32 hashConfig(CSpan<VColorAttachment>, const VDepthAttachment *);

	CSpan<VColorAttachment> colors() const { return m_colors; }
	const Maybe<VDepthAttachment> &depth() const { return m_depth; }

  private:
	friend class VulkanDevice;
	VulkanRenderPass(VkRenderPass, VObjectId);
	~VulkanRenderPass();

	StaticVector<VColorAttachment, max_colors> m_colors;
	Maybe<VDepthAttachment> m_depth;
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
	VulkanSampler(VkSampler, VObjectId, const VSamplerSetup &);
	~VulkanSampler();

	VSamplerSetup m_params;
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