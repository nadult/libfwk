// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/enum_flags.h"
#include "fwk/tag_id.h"

// TODO: move it to vulkan_internal.h; But only after most of the Vulkan definitions are hidden
// Otherwise it makes no sense
#include <vulkan/vulkan.h>

namespace fwk {

DEFINE_ENUM(VTag, cmd, device, window, physical_device, queue_family);

using VDeviceId = TagId<VTag::device, u8>;
using VWindowId = TagId<VTag::window, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;
using VCommandId = TagId<VTag::cmd, u32>;

class VObjectId;

struct VulkanVersion {
	bool operator<(const VulkanVersion &rhs) const {
		return tie(major, minor, patch) < tie(rhs.major, rhs.minor, rhs.patch);
	}

	int major, minor, patch;
};

DEFINE_ENUM(VTypeId, buffer, buffer_view, command_pool, command_buffer, descriptor_pool,
			descriptor_set_layout, fence, framebuffer, image, image_view, pipeline, pipeline_layout,
			render_pass, sampler, semaphore, shader_module, swap_chain);

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

DEFINE_ENUM(VCommandPoolFlag, transient, reset_command, protected_);
using VCommandPoolFlags = EnumFlags<VCommandPoolFlag>;

DEFINE_ENUM(VBufferUsage, transfer_src, transfer_dst, uniform_texel_buffer, storage_texel_buffer,
			uniform_buffer, storage_buffer, index_buffer, vertex_buffer, indirect_buffer);
using VBufferUsageFlags = EnumFlags<VBufferUsage>;

DEFINE_ENUM(VImageUsage, transfer_src, transfer_dst, sampled, storage, color_attachment,
			depth_stencil_attachment, transient_attachment, input_attachment);
using VImageUsageFlags = EnumFlags<VImageUsage>;

DEFINE_ENUM(VImageLayout, undefined, general, color_attachment, depth_stencil_attachment,
			depth_stencil_read_only, shader_read_only, transfer_src, transfer_dst, preinitialized);

DEFINE_ENUM(VShaderStage, vertex, tess_control, tess_eval, geometry, fragment, compute);
using VShaderStageFlags = EnumFlags<VShaderStage>;

DEFINE_ENUM(VDescriptorType, sampler, combined_image_sampler, sampled_image, storage_image,
			uniform_texel_buffer, storage_texel_buffer, uniform_buffer, storage_buffer,
			uniform_buffer_dynamic, storage_buffer_dynamic, input_attachment);

DEFINE_ENUM(VPrimitiveTopology, point_list, line_list, line_strip, triangle_list, triangle_strip,
			triangle_fan);
DEFINE_ENUM(VertexInputRate, vertex, instance);

DEFINE_ENUM(VTexFilter, nearest, linear);
DEFINE_ENUM(VTexAddress, repeat, mirror_repeat, clamp_to_edge, clamp_to_border,
			mirror_clamp_to_edge);

DEFINE_ENUM(VDeviceFeature, memory_budget);
using VDeviceFeatures = EnumFlags<VDeviceFeature>;

DEFINE_ENUM(VMemoryBlockType, slab, unmanaged, frame, invalid);

struct VMemoryBlockId {
	VMemoryBlockId(VMemoryBlockType type, VMemoryDomain domain, u16 zone_id, u32 block_identifier)
		: value(u64(block_identifier) | (u64(type) << 32) | (u64(domain) << 40) |
				(u64(zone_id) << 48)) {}
	VMemoryBlockId() : VMemoryBlockId(VMemoryBlockType::invalid, VMemoryDomain(0), 0, 0) {}

	u16 zoneId() const { return u16(value >> 48); }
	u32 blockIdentifier() const { return u32(value & 0xffffffff); }
	VMemoryDomain domain() const { return VMemoryDomain((value >> 40) & 0xff); }
	VMemoryBlockType type() const { return VMemoryBlockType((value >> 32) & 0xff); }
	bool valid() const { return type() != VMemoryBlockType::invalid; }

	u64 value;
};

struct VMemoryBlock {
	VMemoryBlockId id;
	VkDeviceMemory handle = nullptr;
	u32 offset = 0, size = 0;
};

// TODO: move to internal
inline auto toVk(VShaderStage stage) { return VkShaderStageFlagBits(1u << int(stage)); }
inline auto toVk(VShaderStageFlags flags) { return VkShaderStageFlags(flags.bits); }
inline auto toVk(VDescriptorType type) { return VkDescriptorType(type); }
inline auto toVk(VPrimitiveTopology type) { return VkPrimitiveTopology(type); }
inline auto toVk(VImageUsageFlags usage) { return VkImageUsageFlags{usage.bits}; }
inline auto toVk(VBufferUsageFlags usage) { return VkBufferUsageFlags{usage.bits}; }
inline auto toVk(VCommandPoolFlags flags) { return VkCommandPoolCreateFlagBits(flags.bits); }
inline auto toVk(VImageLayout layout) { return VkImageLayout(layout); }
inline auto toVk(VMemoryFlags flags) { return VkMemoryPropertyFlags(flags.bits); }

struct VSamplingParams {
	VTexFilter mag_filter = VTexFilter::nearest;
	VTexFilter min_filter = VTexFilter::nearest;
	Maybe<VTexFilter> mipmap_filter;
	u8 max_anisotropy_samples = 1;
	array<VTexAddress, 3> address_mode = {VTexAddress::repeat, VTexAddress::repeat,
										  VTexAddress::repeat};
};

class VulkanDevice;
class VulkanInstance;
class VulkanWindow;
class VulkanRenderGraph;
class VulkanMemoryManager;
struct VulkanPhysicalDeviceInfo;

// TODO: PVInstance, PVDevice? PV sucks, but what should I use instead ? VBufferPtr ?
class VInstanceRef;
class VDeviceRef;
class VWindowRef;

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

inline VkRect2D toVkRect(IRect rect) {
	return VkRect2D{.offset = {rect.min().x, rect.min().y},
					.extent = {uint(rect.width()), uint(rect.height())}};
}

}
