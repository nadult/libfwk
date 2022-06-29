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
	int major, minor, patch;
};

DEFINE_ENUM(VTypeId, buffer, command_pool, command_buffer, device_memory, descriptor_pool,
			descriptor_set_layout, fence, framebuffer, image, image_view, pipeline, pipeline_layout,
			render_pass, sampler, semaphore, shader_module, swap_chain);

DEFINE_ENUM(VMemoryFlag, device_local, host_visible, host_coherent, host_cached, lazily_allocated,
			protected_);
using VMemoryFlags = EnumFlags<VMemoryFlag>;

DEFINE_ENUM(VBufferUsage, transfer_src, transfer_dst, uniform_texel_buffer, storage_texel_buffer,
			uniform_buffer, storage_buffer, index_buffer, vertex_buffer, indirect_buffer);
using VBufferUsageFlags = EnumFlags<VBufferUsage>;

DEFINE_ENUM(VImageUsage, transfer_src, transfer_dst, sampled, storage, color_attachment,
			depth_stencil_attachment, transient_attachment, input_attachment);
using VImageUsageFlags = EnumFlags<VImageUsage>;

DEFINE_ENUM(VImageLayout, undefined, general, color_attachment, depth_stencil_attachment,
			depth_stencil_read_only, shader_read_only, transfer_src, transfer_dst, preinitialized);

// TODO: move to internal
inline VkImageUsageFlags toVk(VImageUsageFlags usage) { return VkImageUsageFlags{usage.bits}; }
inline VkBufferUsageFlags toVk(VBufferUsageFlags usage) { return VkBufferUsageFlags{usage.bits}; }
inline VkImageLayout toVk(VImageLayout layout) { return VkImageLayout(layout); }

DEFINE_ENUM(VTexFilter, nearest, linear);
DEFINE_ENUM(VTexAddress, repeat, mirror_repeat, clamp_to_edge, clamp_to_border,
			mirror_clamp_to_edge);

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
