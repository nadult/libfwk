// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/tag_id.h"

// TODO: move it to vulkan_internal.h; But only after most of the Vulkan definitions are hidden
// Otherwise it makes no sense
#include <vulkan/vulkan.h>

namespace fwk {

DEFINE_ENUM(VTag, cmd, ds, dsl, device, window, physical_device, queue_family);

struct VulkanLimits {
	static constexpr int max_color_attachments = 8;
	static constexpr int max_descr_sets = 32;
	static constexpr int max_descr_bindings = 1024 * 1024;

	static constexpr int max_descr_set_layouts = 32 * 1024;
	static constexpr int max_descr_sets_per_layout = 64 * 1024;

	static constexpr int num_swap_frames = 2;
};

using VDeviceId = TagId<VTag::device, u8>;
using VWindowId = TagId<VTag::window, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;
using VCommandId = TagId<VTag::cmd, u32>;

using VDescriptorSetLayoutId = TagId<VTag::dsl, u16>;
using VDescriptorSetId = TagId<VTag::ds, u32>;

using VDSLId = VDescriptorSetLayoutId;
using VDSId = VDescriptorSetId;

class VObjectId;

struct VulkanVersion {
	bool operator<(const VulkanVersion &rhs) const {
		return tie(major, minor, patch) < tie(rhs.major, rhs.minor, rhs.patch);
	}

	int major, minor, patch;
};

DEFINE_ENUM(VTypeId, buffer, buffer_view, framebuffer, image, image_view, pipeline, pipeline_layout,
			render_pass, sampler, shader_module, swap_chain);

// device: fastest memory with device_local (always available)
// host: fastest memory with host_visible (always available)
// temporary: device_local + host_visible
DEFINE_ENUM(VMemoryDomain, device, host, temporary);

// TODO: sometimes device domain can be mapped too
constexpr inline bool canBeMapped(VMemoryDomain domain) { return domain != VMemoryDomain::device; }

// frame: object will only be used during current frame
// device: object will be stored in device memory
// host: object will be stored in host memory
DEFINE_ENUM(VMemoryUsage, frame, device, host);

DEFINE_ENUM(VMemoryFlag, device_local, host_visible, host_coherent, host_cached, lazily_allocated,
			protected_, device_coherent_amd, device_uncached_amd);
using VMemoryFlags = EnumFlags<VMemoryFlag>;

DEFINE_ENUM(VQueueCap, graphics, compute, transfer);
using VQueueCaps = EnumFlags<VQueueCap>;

DEFINE_ENUM(VCommandPoolFlag, transient, reset_command, protected_);
using VCommandPoolFlags = EnumFlags<VCommandPoolFlag>;

DEFINE_ENUM(VBufferUsage, transfer_src, transfer_dst, uniform_texel_buffer, storage_texel_buffer,
			uniform_buffer, storage_buffer, index_buffer, vertex_buffer, indirect_buffer);
using VBufferUsageFlags = EnumFlags<VBufferUsage>;

DEFINE_ENUM(VImageUsage, transfer_src, transfer_dst, sampled, storage, color_att, depth_stencil_att,
			transient_att, input_att);
using VImageUsageFlags = EnumFlags<VImageUsage>;

DEFINE_ENUM(VImageLayout, undefined, general, color_att, depth_stencil_att, depth_stencil_ro,
			shader_ro, transfer_src, transfer_dst, preinitialized, present_src);

DEFINE_ENUM(VShaderStage, vertex, tess_control, tess_eval, geometry, fragment, compute);
using VShaderStages = EnumFlags<VShaderStage>;

DEFINE_ENUM(VDescriptorType, sampler, combined_image_sampler, sampled_image, storage_image,
			uniform_texel_buffer, storage_texel_buffer, uniform_buffer, storage_buffer,
			uniform_buffer_dynamic, storage_buffer_dynamic, input_att);
using VDescriptorTypes = EnumFlags<VDescriptorType>;

DEFINE_ENUM(VDescriptorPoolFlag, free_descriptor_set, update_after_bind, host_only);
using VDescriptorPoolFlags = EnumFlags<VDescriptorPoolFlag>;

DEFINE_ENUM(VPrimitiveTopology, point_list, line_list, line_strip, triangle_list, triangle_strip,
			triangle_fan);
DEFINE_ENUM(VertexInputRate, vertex, instance);

DEFINE_ENUM(VTexFilter, nearest, linear);
DEFINE_ENUM(VTexAddress, repeat, mirror_repeat, clamp_to_edge, clamp_to_border,
			mirror_clamp_to_edge);

DEFINE_ENUM(VDeviceFeature, memory_budget);
using VDeviceFeatures = EnumFlags<VDeviceFeature>;

DEFINE_ENUM(VMemoryBlockType, slab, unmanaged, frame, invalid);

DEFINE_ENUM(VPresentMode, immediate, mailbox, fifo, fifo_relaxed);

DEFINE_ENUM(VLoadOp, load, clear, dont_care, none);
DEFINE_ENUM(VStoreOp, store, dont_care, none);

DEFINE_ENUM(VBlendFactor, zero, one, src_color, one_minus_src_color, dst_color, one_minus_dst_color,
			src_alpha, one_minus_src_alpha, dst_alpha, one_minus_dst_alpha, constant_color,
			one_minus_constant_color, constant_alpha, one_minus_constant_alpha, src_alpha_saturate,
			src1_color, one_minus_src1_color, src1_alpha, one_minus_src1_alpha);

DEFINE_ENUM(VBlendOp, add, subtract, reverse_subtract, min, max);
DEFINE_ENUM(VColorComponent, red, green, blue, alpha);
using VColorComponents = EnumFlags<VColorComponent>;

DEFINE_ENUM(VPolygonMode, fill, line, point);
DEFINE_ENUM(VCull, front, back);
DEFINE_ENUM(VFrontFace, ccw, cw);
DEFINE_ENUM(VRasterFlag, discard, primitive_restart);
using VCullMode = EnumFlags<VCull>;
using VRasterFlags = EnumFlags<VRasterFlag>;

DEFINE_ENUM(VDynamic, viewport, scissor, line_width, depth_bias, blend_constants, depth_bounds,
			stencil_compare_mask, stencil_write_mask, stencil_reference);
using VDynamicState = EnumFlags<VDynamic>;

DEFINE_ENUM(VCompareOp, never, less, equal, less_equal, greater, not_equal, greater_equal, always);
DEFINE_ENUM(VDepthFlag, test, write, bounds_test, bias, clamp);
using VDepthFlags = EnumFlags<VDepthFlag>;

struct VMemoryBlockId {
	using Type = VMemoryBlockType;
	VMemoryBlockId(Type type, VMemoryDomain domain, u16 zone_id, u32 block_identifier)
		: encoded_value(u64(block_identifier) | (u64(type) << 32) | (u64(domain) << 40) |
						(u64(zone_id) << 48)) {}
	VMemoryBlockId() : VMemoryBlockId(Type::invalid, VMemoryDomain(0), 0, 0) {}

	u16 zoneId() const { return u16(encoded_value >> 48); }
	u32 blockIdentifier() const { return u32(encoded_value & 0xffffffff); }
	VMemoryDomain domain() const { return VMemoryDomain((encoded_value >> 40) & 0xff); }
	Type type() const { return Type((encoded_value >> 32) & 0xff); }
	bool valid() const { return type() != Type::invalid; }
	bool requiresFree() const { return isOneOf(type(), Type::slab, Type::unmanaged); }

	u64 encoded_value;
};

struct VMemoryBlock {
	VMemoryBlockId id;
	VkDeviceMemory handle = nullptr;
	u32 offset = 0, size = 0;
};

struct VSamplerSetup {
	VTexFilter mag_filter = VTexFilter::nearest;
	VTexFilter min_filter = VTexFilter::nearest;
	Maybe<VTexFilter> mipmap_filter;
	u8 max_anisotropy_samples = 1;
	array<VTexAddress, 3> address_mode = {VTexAddress::repeat, VTexAddress::repeat,
										  VTexAddress::repeat};
};

DEFINE_ENUM(VColorSyncStd, clear, clear_present, present);

// TODO: move out of base
struct VColorSync {
	using Std = VColorSyncStd;
	VColorSync(Std std)
		: load_op(std == Std::present ? VLoadOp::load : VLoadOp::clear), store_op(VStoreOp::store),
		  initial_layout(std == Std::present ? VImageLayout::color_att : VImageLayout::undefined),
		  final_layout(std == Std::clear ? VImageLayout::color_att : VImageLayout::present_src) {}
	VColorSync(VLoadOp load, VStoreOp store, VImageLayout initial_layout, VImageLayout final_layout)
		: load_op(load), store_op(store), initial_layout(initial_layout),
		  final_layout(final_layout) {}
	VColorSync()
		: load_op(VLoadOp::load), store_op(VStoreOp::store),
		  initial_layout(VImageLayout::color_att), final_layout(VImageLayout::color_att) {}

	FWK_TIE_MEMBERS(load_op, store_op, initial_layout, final_layout);
	FWK_TIED_COMPARES(VColorSync);

	VLoadOp load_op;
	VStoreOp store_op;
	VImageLayout initial_layout;
	VImageLayout final_layout;
};

struct VDepthSync {
	VDepthSync(VLoadOp load, VStoreOp store, VImageLayout initial_layout, VImageLayout final_layout,
			   VLoadOp stencil_load = VLoadOp::dont_care,
			   VStoreOp stencil_store = VStoreOp::dont_care)
		: load_op(load), store_op(store), stencil_load_op(stencil_load),
		  stencil_store_op(stencil_store), initial_layout(initial_layout),
		  final_layout(final_layout) {}
	VDepthSync()
		: VDepthSync(VLoadOp::load, VStoreOp::store, VImageLayout::depth_stencil_att,
					 VImageLayout::depth_stencil_att, VLoadOp::dont_care, VStoreOp::dont_care) {}

	FWK_TIE_MEMBERS(load_op, store_op, stencil_load_op, stencil_store_op, initial_layout,
					final_layout);
	FWK_TIED_COMPARES(VDepthSync);

	VLoadOp load_op;
	VStoreOp store_op;
	VLoadOp stencil_load_op;
	VStoreOp stencil_store_op;
	VImageLayout initial_layout;
	VImageLayout final_layout;
};

struct VColorAttachment {
	VColorAttachment(VkFormat format, uint num_samples = 1, VColorSync sync = {})
		: format(format), num_samples(num_samples), sync(sync) {}

	FWK_TIE_MEMBERS(format, num_samples, sync);
	FWK_TIED_COMPARES(VColorAttachment);

	VkFormat format;
	uint num_samples;
	VColorSync sync;
};

struct VDepthAttachment {
	VDepthAttachment(VkFormat format, uint num_samples = 1, VDepthSync sync = {})
		: format(format), num_samples(num_samples), sync(sync) {}

	FWK_TIE_MEMBERS(format, num_samples, sync);
	FWK_TIED_COMPARES(VDepthAttachment);

	VkFormat format;
	uint num_samples;
	VDepthSync sync;
};

struct VQueue {
	VkQueue handle = nullptr;
	VQueueFamilyId family_id = VQueueFamilyId(0);
	VQueueCaps caps;
};

class VulkanDevice;
class VulkanInstance;
class VulkanWindow;
class VulkanRenderGraph;
class VulkanMemoryManager;
class VulkanDescriptorManager;
struct VulkanPhysicalDeviceInfo;

// TODO: PVInstance, PVDevice? PV sucks, but what should I use instead ? VBufferPtr ?
class VInstanceRef;
class VDeviceRef;
class VWindowRef;

struct VDescriptorSet;

struct DescriptorBindingInfo; // TODO: name
struct VColorAttachment;
struct VDepthAttachment;

template <class T> class VPtr;

// TODO: Better name than 'Wrapper'
template <class> struct VulkanTypeInfo;
template <class> struct VulkanHandleInfo;
#define CASE_TYPE(UpperCase, VkName, lower_case)                                                   \
	class Vulkan##UpperCase;                                                                       \
	template <> struct VulkanHandleInfo<VkName> {                                                  \
		static constexpr VTypeId type_id = VTypeId::lower_case;                                    \
		using Type = Vulkan##UpperCase;                                                            \
	};                                                                                             \
	template <> struct VulkanTypeInfo<Vulkan##UpperCase> {                                         \
		static constexpr VTypeId type_id = VTypeId::lower_case;                                    \
		using Handle = VkName;                                                                     \
	};                                                                                             \
	using PV##UpperCase = VPtr<VkName>;
#include "fwk/vulkan/vulkan_type_list.h"

template <class Enum, class Bit>
VkFlags translateFlags(EnumFlags<Enum> flags, const EnumMap<Enum, Bit> &bit_map) {
	VkFlags out = 0;
	for(auto flag : flags)
		out |= bit_map[flag];
	return out;
}

inline VkExtent2D toVkExtent(int2 extent) {
	PASSERT(extent.x >= 0 && extent.y >= 0);
	return {uint(extent.x), uint(extent.y)};
}

inline int2 fromVk(VkExtent2D extent) { return int2(extent.width, extent.height); }

inline VkRect2D toVkRect(IRect rect) {
	return VkRect2D{.offset = {rect.min().x, rect.min().y},
					.extent = {uint(rect.width()), uint(rect.height())}};
}

}
