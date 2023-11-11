// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_map.h"
#include "fwk/vulkan_base.h"

#include <vulkan/vulkan.h>

namespace fwk {

inline auto toVk(VShaderStage stage) { return VkShaderStageFlagBits(1u << int(stage)); }
inline auto toVk(VShaderStages flags) { return VkShaderStageFlags(flags.bits); }
inline auto toVk(VDescriptorType type) { return VkDescriptorType(type); }
inline auto toVk(VDescriptorPoolFlags flags) { return VkDescriptorPoolCreateFlagBits(flags.bits); }
inline auto toVk(VPrimitiveTopology type) { return VkPrimitiveTopology(type); }
inline auto toVk(VImageUsageFlags usage) { return VkImageUsageFlags{usage.bits}; }
inline auto toVk(VBufferUsageFlags usage) { return VkBufferUsageFlags{usage.bits}; }
inline auto toVk(VCommandPoolFlags flags) { return VkCommandPoolCreateFlagBits(flags.bits); }
inline auto toVk(VMemoryFlags flags) { return VkMemoryPropertyFlags(flags.bits); }
inline auto toVk(VPresentMode mode) { return VkPresentModeKHR(mode); }
inline auto toVk(VLoadOp op) {
	return op == VLoadOp::none ? VK_ATTACHMENT_LOAD_OP_NONE_EXT : VkAttachmentLoadOp(op);
}
inline auto toVk(VStoreOp op) {
	return op == VStoreOp::none ? VK_ATTACHMENT_STORE_OP_NONE_EXT : VkAttachmentStoreOp(op);
}
inline auto toVk(VImageLayout layout) {
	if(layout == VImageLayout::present_src)
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	if(layout >= VImageLayout::depth_att) {
		uint offset =
			uint(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) - uint(VImageLayout::depth_att);
		return VkImageLayout(uint(layout) + offset);
	}
	if(layout >= VImageLayout::depth_ro_stencil_att) {
		uint offset = uint(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL) -
					  uint(VImageLayout::depth_ro_stencil_att);
		return VkImageLayout(uint(layout) + offset);
	}
	return VkImageLayout(layout);
}

inline auto toVk(VPipeStages stages) { return VkPipelineStageFlags(stages.bits); }
inline auto toVk(VBlendFactor factor) { return VkBlendFactor(factor); }
inline auto toVk(VBlendOp op) { return VkBlendOp(op); }
inline auto toVk(VColorComponents components) { return VkColorComponentFlags(components.bits); }
inline auto toVk(VPolygonMode mode) { return VkPolygonMode(mode); }
inline auto toVk(VCullMode mode) { return VkCullModeFlags(mode.bits); }
inline auto toVk(VFrontFace face) { return VkFrontFace(face); }
inline auto toVk(VCompareOp op) { return VkCompareOp(op); }
inline auto toVk(VDynamic dynamic) { return VkDynamicState(dynamic); }
inline auto toVk(VQueueCaps caps) { return VkQueueFlags(caps.bits); }
inline auto toVk(VBindPoint bind_point) { return VkPipelineBindPoint(bind_point); }
inline auto toVk(VDepthStencilFormat format) {
	return VkFormat(uint(VK_FORMAT_D16_UNORM) + uint(format));
}
inline auto fromVkDepthStencilFormat(VkFormat format) {
	return format >= VK_FORMAT_D16_UNORM && format < VK_FORMAT_D32_SFLOAT_S8_UINT ?
			   VDepthStencilFormat(uint(format) - uint(VK_FORMAT_D16_UNORM)) :
			   Maybe<VDepthStencilFormat>();
}

VkFormat toVk(VFormat);
Maybe<VFormat> fromVk(VkFormat);

inline VkExtent2D toVkExtent(int2 extent) {
	PASSERT(extent.x >= 0 && extent.y >= 0);
	return {uint(extent.x), uint(extent.y)};
}

inline int2 fromVk(VkExtent2D extent) { return int2(extent.width, extent.height); }

inline VkRect2D toVkRect(IRect rect) {
	return VkRect2D{.offset = {rect.min().x, rect.min().y},
					.extent = {uint(rect.width()), uint(rect.height())}};
}

VkCommandBuffer allocVkCommandBuffer(VkDevice, VkCommandPool pool);
VkSemaphore createVkSemaphore(VkDevice, bool is_signaled = false);
VkFence createVkFence(VkDevice, bool is_signaled = false);
VkEvent createVkEvent(VkDevice, bool device_only = false);

VkCommandPool createVkCommandPool(VkDevice, VQueueFamilyId queue_family_id,
								  VCommandPoolFlags flags);
VkDescriptorPool createVkDescriptorPool(VkDevice, EnumMap<VDescriptorType, uint> type_counts,
										uint set_count, VDescriptorPoolFlags flags = none);

string translateVkResult(VkResult);
Error makeVkError(const char *file, int line, VkResult, const char *function_name);
[[noreturn]] void fatalVkError(const char *file, int line, VkResult, const char *function_name);

#define FWK_VK_ERROR(func_name, result) makeVkError(__FILE__, __LINE__, result, func_name);
#define FWK_VK_FATAL(func_name, result) fatalVkError(__FILE__, __LINE__, result, func_name);

// Calls given function with passed arguments, returns ERROR on error
#define FWK_VK_EXPECT_CALL(func, ...)                                                              \
	{                                                                                              \
		auto result = func(__VA_ARGS__);                                                           \
		if(result < 0)                                                                             \
			return FWK_VK_ERROR(#func, result);                                                    \
	}

// Calls given function with passed arguments, stops program with FATAL on error
#define FWK_VK_CALL(func, ...)                                                                     \
	{                                                                                              \
		auto result = func(__VA_ARGS__);                                                           \
		if(result < 0)                                                                             \
			FWK_VK_FATAL(#func, result);                                                           \
	}

}
