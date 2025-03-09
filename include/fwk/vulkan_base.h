// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/tag_id.h"

#define VK_NO_PROTOTYPES

// TODO: drop this dependency
#include <vulkan/vulkan_core.h>

namespace fwk {

DEFINE_ENUM(VTag, cmd, ds, dsl, device, window, physical_device, queue_family, download);

struct VulkanLimits {
	static constexpr int max_color_attachments = 8;
	static constexpr int max_descr_sets = 32;
	static constexpr int max_descr_bindings = 64;

	static constexpr int max_descr_set_layouts = 32 * 1024;
	static constexpr int max_descr_sets_per_layout = 64 * 1024;

	static constexpr int num_swap_frames = 2;

	static constexpr int max_image_size = 32 * 1024;
	static constexpr int max_image_samples = 32;
	static constexpr int max_mip_levels = 16;
};

using VDeviceId = TagId<VTag::device, u8>;
using VWindowId = TagId<VTag::window, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;
using VCommandId = TagId<VTag::cmd, u32>;
using VDownloadId = TagId<VTag::download, u32>;

using VDescriptorSetLayoutId = TagId<VTag::dsl, u16>;
using VDescriptorSetId = TagId<VTag::ds, u32>;

using VDSLId = VDescriptorSetLayoutId;
using VDSId = VDescriptorSetId;

class VObjectId;

struct VulkanVersion {
	VulkanVersion(int major = 1, int minor = 0, int patch = 0)
		: major(major), minor(minor), patch(patch) {}

	bool operator<(const VulkanVersion &rhs) const {
		return tie(major, minor, patch) < tie(rhs.major, rhs.minor, rhs.patch);
	}

	int major, minor, patch;
};

DEFINE_ENUM(VVendorId, intel, nvidia, amd, unknown);

DEFINE_ENUM(VTypeId, accel_struct, buffer, buffer_view, framebuffer, image, image_view, pipeline,
			pipeline_layout, render_pass, sampler, shader_module, swap_chain);

// device: fastest memory with device_local (always available)
// host: fastest memory with host_visible (always available)
// temporary: device_local + host_visible
DEFINE_ENUM(VMemoryDomain, device, host, temporary);
using VMemoryDomains = EnumFlags<VMemoryDomain>;

// TODO: sometimes device domain can be mapped too
constexpr inline bool canBeMapped(VMemoryDomain domain) { return domain != VMemoryDomain::device; }

// frame: object will only be used during current frame
// device: object will be stored in device memory
// host: object will be stored in host memory
// TODO: cleanup: frame / temporary
DEFINE_ENUM(VMemoryUsage, frame, temporary, device, host);

DEFINE_ENUM(VMemoryFlag, device_local, host_visible, host_coherent, host_cached, lazily_allocated,
			protected_, device_coherent_amd, device_uncached_amd);
using VMemoryFlags = EnumFlags<VMemoryFlag>;

DEFINE_ENUM(VQueueCap, graphics, compute, transfer);
using VQueueCaps = EnumFlags<VQueueCap>;

DEFINE_ENUM(VCommandPoolFlag, transient, reset_command, protected_);
using VCommandPoolFlags = EnumFlags<VCommandPoolFlag>;

DEFINE_ENUM(VBindPoint, graphics, compute);

DEFINE_ENUM(VBufferUsage, transfer_src, transfer_dst, uniform_texel_buffer, storage_texel_buffer,
			uniform_buffer, storage_buffer, index_buffer, vertex_buffer, indirect_buffer,
			device_address, accel_struct_build_input_read_only, accel_struct_storage);
using VBufferUsageFlags = EnumFlags<VBufferUsage>;

DEFINE_ENUM(VImageUsage, transfer_src, transfer_dst, sampled, storage, color_att, depth_stencil_att,
			transient_att, input_att);
using VImageUsageFlags = EnumFlags<VImageUsage>;

DEFINE_ENUM(VImageLayout, undefined, general, color_att, depth_stencil_att, depth_stencil_ro,
			shader_ro, transfer_src, transfer_dst, preinitialized, depth_ro_stencil_att,
			depth_stencil_ro_att, depth_att, depth_ro, stencil_att, stencil_ro, present_src);

DEFINE_ENUM(VShaderStage, vertex, tess_control, tess_eval, geometry, fragment, compute);
using VShaderStages = EnumFlags<VShaderStage>;

DEFINE_ENUM(VDescriptorType, sampler, combined_image_sampler, sampled_image, storage_image,
			uniform_texel_buffer, storage_texel_buffer, uniform_buffer, storage_buffer,
			uniform_buffer_dynamic, storage_buffer_dynamic, input_att, accel_struct);
using VDescriptorTypes = EnumFlags<VDescriptorType>;

DEFINE_ENUM(VDescriptorPoolFlag, free_descriptor_set, update_after_bind, host_only);
using VDescriptorPoolFlags = EnumFlags<VDescriptorPoolFlag>;

DEFINE_ENUM(VPrimitiveTopology, point_list, line_list, line_strip, triangle_list, triangle_strip,
			triangle_fan);
DEFINE_ENUM(VertexInputRate, vertex, instance);

DEFINE_ENUM(VTexFilter, nearest, linear);
DEFINE_ENUM(VTexAddress, repeat, mirror_repeat, clamp_to_edge, clamp_to_border,
			mirror_clamp_to_edge);

DEFINE_ENUM(VDeviceFeature, memory_budget, subgroup_size_control, shader_clock, ray_tracing,
			descriptor_update_after_bind);
using VDeviceFeatures = EnumFlags<VDeviceFeature>;

DEFINE_ENUM(VMemoryBlockType, slab, unmanaged, frame, invalid);
using VMemoryBlockTypes = EnumFlags<VMemoryBlockType>;

DEFINE_ENUM(VPresentMode, immediate, mailbox, fifo, fifo_relaxed);

DEFINE_ENUM(VLoadOp, load, clear, dont_care, none);
DEFINE_ENUM(VStoreOp, store, dont_care, none);

DEFINE_ENUM(VPipeStage, top, draw_indirect, vertex_input, vertex_shader, tess_control_shader,
			tess_evaluation_shader, geometry_shader, fragment_shader, early_fragment_tests,
			late_fragment_tests, color_att_output, compute_shader, transfer, bottom, host,
			all_graphics, all_commands);
using VPipeStages = EnumFlags<VPipeStage>;

DEFINE_ENUM(VAccess, indirect_command_read, index_read, vertex_attribute_read, uniform_read,
			input_attachment_read, shader_read, shader_write, color_att_read, color_att_write,
			depth_stencil_att_read, depth_stencil_att_write, transfer_read, transfer_write,
			host_read, host_write, memory_read, memory_write);
using VAccessFlags = EnumFlags<VAccess>;

DEFINE_ENUM(VBaseFormat, r8, rg8, rgb8, bgr8, rgba8, bgra8, abgr8, a2rgb10, a2bgr10, r16, rg16,
			rgb16, rgba16, r32, rg32, rgb32, rgba32, b10g11r11_ufloat, e5r9g9b9_ufloat, bc1_rgb,
			bc1_rgba, bc2_rgba, bc3_rgba, bc4_r, bc5_rg, bc6h_rgb, bc7_rgba);
using VBaseFormats = EnumFlags<VBaseFormat>;

constexpr bool isBlock(VBaseFormat format) {
	return format >= VBaseFormat::bc1_rgb && format <= VBaseFormat::bc7_rgba;
}

int unitByteSize(VBaseFormat);
constexpr int unitSize(VBaseFormat format) { return isBlock(format) ? 4 : 1; }

//   unorm: unsigned normalized values in the range [ 0, 1]
//   snorm:   signed normalized values in the range [-1, 1]
//    uint: unsigned integer values in the range [       0, 2^n - 1    ]
//    sint:   signed integer values in the range [-2^(n-1), 2^(n-1) - 1]
//  ufloat: unsigned floating-point
//  sfloat:   signed floating-point
//    srgb: unorm with RGB additionally using sRGB nonlinear encoding
DEFINE_ENUM(VNumericFormat, unorm, snorm, uint, sint, ufloat, sfloat, srgb);

DEFINE_ENUM(VColorFormat,
			//  8-bit R8
			r8_unorm, r8_snorm, r8_uint, r8_sint, r8_srgb,
			// 16-bit (R8, G8)
			rg8_unorm, rg8_snorm, rg8_uint, rg8_sint, rg8_srgb,
			// 24-bit (R8, G8, B8)
			rgb8_unorm, rgb8_snorm, rgb8_uint, rgb8_sint, rgb8_srgb,
			// 24-bit (B8, G8, R8)
			bgr8_unorm, bgr8_snorm, bgr8_uint, bgr8_sint, bgr8_srgb,
			// 32-bit (R8, G8, B8, A8)
			rgba8_unorm, rgba8_snorm, rgba8_uint, rgba8_sint, rgba8_srgb,
			// 32-bit (B8, G8, R8, A8)
			bgra8_unorm, bgra8_snorm, bgra8_uint, bgra8_sint, bgra8_srgb,
			// 32-bit (A8, B8, G8, R8), packed
			abgr8_unorm, abgr8_snorm, abgr8_uint, abgr8_sint, abgr8_srgb,

			// 32-bit (A2, R10, G10, B10), packed
			a2rgb10_unorm, a2rgb10_snorm, a2rgb10_uint, a2rgb10_sint,
			// 32-bit (A2, B10, G10, R10), packed
			a2bgr10_unorm, a2bgr10_snorm, a2bgr10_uint, a2bgr10_sint,

			// 16-bit R16
			r16_unorm, r16_snorm, r16_uint, r16_sint, r16_sfloat,
			// 32-bit (R16, G16)
			rg16_unorm, rg16_snorm, rg16_uint, rg16_sint, rg16_sfloat,
			// 48-bit (R16, G16, B16)
			rgb16_unorm, rgb16_snorm, rgb16_uint, rgb16_sint, rgb16_sfloat,
			// 64-bit (R16, G16, B16, A16)
			rgba16_unorm, rgba16_snorm, rgba16_uint, rgba16_sint, rgba16_sfloat,

			//  32-bit R32
			r32_uint, r32_sint, r32_sfloat,
			//  64-bit (R32, G32)
			rg32_uint, rg32_sint, rg32_sfloat,
			//  96-bit (R32, G32, B32)
			rgb32_uint, rgb32_sint, rgb32_sfloat,
			// 128-bit (R32, G32, B32, A32)
			rgba32_uint, rgba32_sint, rgba32_sfloat,

			// 32-bit packed formats
			b10g11r11_ufloat, e5r9g9b9_ufloat,

			//  64-bit 4x4 BC1 (DXT1)
			bc1_rgb_unorm, bc1_rgb_srgb, bc1_rgba_unorm, bc1_rgba_srgb,
			// 128-bit 4x4 BC2 & BC3 (DXT3 & DXT5)
			bc2_rgba_unorm, bc2_rgba_srgb, bc3_rgba_unorm, bc3_rgba_srgb,
			//  64-bit 4x4 BC4 (single channel)
			bc4_r_unorm, bc4_r_snorm,
			// 128-bit 4x4 BC5 (two channels)
			bc5_rg_unorm, bc5_rg_snorm,
			// 128-bit 4x4 BC6H
			bc6h_rgb_ufloat, bc6h_rgb_sfloat,
			// 128-bit 4x4 BC7
			bc7_rgba_unorm, bc7_rgba_srgb);

VBaseFormat baseFormat(VColorFormat);
VNumericFormat numericFormat(VColorFormat);
Maybe<VColorFormat> makeFormat(VBaseFormat, VNumericFormat);

DEFINE_ENUM(VFormatFeature, sampled_image, storage_image, storage_image_atomic,
			uniform_texel_buffer, storage_texel_buffer, storage_texel_buffer_atomic, vertex_buffer,
			color_attachment, color_attachment_blend, depth_stencil_attachment, blit_src, blit_dst,
			sampled_image_filter_linear, transfer_src, transfer_dst);

using VFormatFeatures = EnumFlags<VFormatFeature>;

struct VFormatSupport {
	VFormatFeatures linear_tiling, optimal_tiling, buffer;
};

constexpr bool isBlock(VColorFormat format) {
	return format >= VColorFormat::bc1_rgb_unorm && format <= VColorFormat::bc7_rgba_srgb;
}

int unitByteSize(VColorFormat);
constexpr int unitSize(VColorFormat format) { return isBlock(format) ? 4 : 1; }

// Formats which have same unit size & unit byte size are compatible.
bool areCompatible(VColorFormat, VColorFormat);

int2 imageBlockSize(VColorFormat format, int2 pixel_size);
int imageByteSize(VColorFormat, int2 pixel_size);

DEFINE_ENUM(VDepthStencilFormat, d16, d24_x8, d32f, s8, d16_s8, d24_s8, d32f_s8);

using VDepthStencilFormats = EnumFlags<VDepthStencilFormat>;
inline bool hasStencil(VDepthStencilFormat format) { return format >= VDepthStencilFormat::s8; }
inline bool hasDepth(VDepthStencilFormat format) { return format != VDepthStencilFormat::s8; }
inline VImageLayout defaultLayout(VDepthStencilFormat format) {
	if(hasDepth(format))
		return hasStencil(format) ? VImageLayout::depth_stencil_att : VImageLayout::depth_att;
	return VImageLayout::stencil_att;
}

inline uint depthBits(VDepthStencilFormat format) {
	return format == VDepthStencilFormat::s8 ? 0 : (uint(format) & 3) * 8 + 16;
}
inline uint stencilBits(VDepthStencilFormat format) { return hasStencil(format) ? 8 : 0; }
inline VNumericFormat depthNumericFormat(VDepthStencilFormat format) {
	return depthBits(format) == 32 ? VNumericFormat::sfloat : VNumericFormat::unorm;
}

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
	VSamplerSetup(VTexFilter mag_filter = VTexFilter::nearest,
				  VTexFilter min_filter = VTexFilter::nearest, Maybe<VTexFilter> mip_filter = none,
				  VTexAddress address_mode = VTexAddress::repeat, uint max_anisotropy_samples = 1);
	VSamplerSetup(VTexFilter mag_filter, VTexFilter min_filter, Maybe<VTexFilter> mip_filter,
				  array<VTexAddress, 3> address_mode, uint max_anisotropy_samples = 1);

	FWK_TIE_MEMBERS(mag_filter, min_filter, mipmap_filter, max_anisotropy_samples, address_mode);
	FWK_TIED_COMPARES(VSamplerSetup);

	VTexFilter mag_filter = VTexFilter::nearest;
	VTexFilter min_filter = VTexFilter::nearest;
	Maybe<VTexFilter> mipmap_filter;
	u8 max_anisotropy_samples = 1;
	array<VTexAddress, 3> address_mode = {VTexAddress::repeat, VTexAddress::repeat,
										  VTexAddress::repeat};
};

struct VSwapChainSetup {
	vector<VkFormat> preferred_formats = {VK_FORMAT_B8G8R8A8_SRGB};
	vector<VkFormat> preferred_depth_formats = {};
	VPresentMode preferred_present_mode = VPresentMode::fifo;
	VImageUsageFlags usage = VImageUsage::color_att;
	VImageLayout initial_layout = VImageLayout::color_att;
};

struct VQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VMemoryManagerSetup {
	// In case of disabled slab / frame allocation, simple unmanaged allocation will be used
	bool enable_slab_allocator = true;
	bool enable_frame_allocator = true;
	bool enable_device_address = false;
};

struct VDeviceSetup {
	vector<string> extensions;
	vector<VQueueSetup> queues;
	VMemoryManagerSetup memory;
	Dynamic<VkPhysicalDeviceFeatures> features;

	// Mechanisms for descriptor updates in libfwk depend on the ability to update them after binding.
	bool allow_descriptor_update_after_bind = true;
};

DEFINE_ENUM(VSimpleSync, clear, clear_present, present, draw);
DEFINE_ENUM(VAttachmentType, color, depth, depth_stencil);
inline bool hasDepth(VAttachmentType type) {
	return isOneOf(type, VAttachmentType::depth, VAttachmentType::depth_stencil);
}

struct VAttachmentSync {
	VAttachmentSync(VLoadOp load_op, VStoreOp store_op, VLoadOp stencil_load_op,
					VStoreOp stencil_store_op, VImageLayout initial_layout,
					VImageLayout final_layout);
	explicit VAttachmentSync(u16 encoded) : encoded(encoded) {}
	static VAttachmentSync make(VSimpleSync, VAttachmentType);

	auto operator<=>(const VAttachmentSync &) const = default;

	VLoadOp loadOp() const { return VLoadOp(encoded & 3); }
	VStoreOp storeOp() const { return VStoreOp((encoded >> 2) & 3); }
	VLoadOp stencilLoadOp() const { return VLoadOp((encoded >> 4) & 3); }
	VStoreOp stencilStoreOp() const { return VStoreOp((encoded >> 6) & 3); }
	VImageLayout initialLayout() const { return VImageLayout((encoded >> 8) & 15); }
	VImageLayout finalLayout() const { return VImageLayout((encoded >> 12) & 15); }

	u16 encoded;
};

struct VAttachment {
	using Type = VAttachmentType;

	VAttachment(VColorFormat, VAttachmentSync, uint num_samples = 1);
	VAttachment(VColorFormat, VSimpleSync, uint num_samples = 1);

	VAttachment(VDepthStencilFormat, VAttachmentSync, uint num_samples = 1);
	VAttachment(VDepthStencilFormat, VSimpleSync, uint num_samples = 1);

	auto operator<=>(const VAttachment &) const = default;

	VColorFormat colorFormat() const {
		PASSERT(type() == Type::color);
		return VColorFormat(encoded & 255);
	}
	VDepthStencilFormat depthStencilFormat() const {
		PASSERT(type() != Type::color);
		return VDepthStencilFormat(encoded & 255);
	}
	Type type() const { return Type((encoded >> 8) & 3); }
	uint numSamples() const { return (encoded >> 10) & 63; }
	u32 hash() const { return fwk::hashU32(encoded); }
	VAttachmentSync sync() const { return VAttachmentSync(u16(encoded >> 16)); }

	u32 encoded;
};

struct VQueue {
	VkQueue handle = nullptr;
	VQueueFamilyId family_id = VQueueFamilyId(0);
	VQueueCaps caps;
};

struct VPushConstantRanges;

class VulkanDevice;
class VulkanInstance;
class VulkanWindow;
class VulkanCommandQueue;
class VulkanQueryManager;
class VulkanMemoryManager;
class VulkanDescriptorManager;
struct VulkanPhysicalDeviceInfo;

// TODO: PVInstance, PVDevice? PV sucks, but what should I use instead ? VBufferPtr ?
class VInstanceRef;
class VDeviceRef;
class VWindowRef;

struct VDescriptorSet;

struct VDescriptorBindingInfo;
struct VAttachmentSync;
struct VAttachment;
struct VPipelineSetup;

template <class T = char> struct VBufferSpan;

template <class T> class VPtr;

template <class> struct VulkanTypeInfo;
template <class> struct VulkanHandleInfo;
#define CASE_TYPE(ClassName, VkName, lower_case)                                                   \
	class ClassName;                                                                               \
	template <> struct VulkanHandleInfo<VkName> {                                                  \
		static constexpr VTypeId type_id = VTypeId::lower_case;                                    \
		using Type = ClassName;                                                                    \
	};                                                                                             \
	template <> struct VulkanTypeInfo<ClassName> {                                                 \
		static constexpr VTypeId type_id = VTypeId::lower_case;                                    \
		using Handle = VkName;                                                                     \
	};
#include "fwk/vulkan/vulkan_type_list.h"

using PVBuffer = VPtr<VulkanBuffer>;
using PVFramebuffer = VPtr<VulkanFramebuffer>;
using PVImage = VPtr<VulkanImage>;
using PVImageView = VPtr<VulkanImageView>;
using PVPipeline = VPtr<VulkanPipeline>;
using PVPipelineLayout = VPtr<VulkanPipelineLayout>;
using PVRenderPass = VPtr<VulkanRenderPass>;
using PVShaderModule = VPtr<VulkanShaderModule>;
using PVSwapChain = VPtr<VulkanSwapChain>;
using PVSampler = VPtr<VulkanSampler>;
using PVAccelStruct = VPtr<VulkanAccelStruct>;

int primitiveCount(VPrimitiveTopology topo, int vertex_count);
}
