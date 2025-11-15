// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define VOLK_IMPLEMENTATION

#include "fwk/vulkan/vulkan_internal.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

// -------------------------------------------------------------------------------------------
// ----------  Low level functions  ----------------------------------------------------------

VkCommandBuffer allocVkCommandBuffer(VkDevice device, VkCommandPool pool) {
	VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	ai.commandPool = pool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	VkCommandBuffer handle;
	FWK_VK_CALL(vkAllocateCommandBuffers, device, &ai, &handle);
	return handle;
}

VkSemaphore createVkSemaphore(VkDevice device) {
	VkSemaphore handle;
	VkSemaphoreCreateInfo ci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	FWK_VK_CALL(vkCreateSemaphore, device, &ci, nullptr, &handle);
	return handle;
}

VkFence createVkFence(VkDevice device, bool is_signaled) {
	VkFence handle;
	VkFenceCreateInfo ci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	ci.flags = is_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
	FWK_VK_CALL(vkCreateFence, device, &ci, nullptr, &handle);
	return handle;
}

VkEvent createVkEvent(VkDevice device, bool device_only) {
	VkEvent handle;
	VkEventCreateInfo ci{VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
	ci.flags = device_only ? VK_EVENT_CREATE_DEVICE_ONLY_BIT : 0;
	FWK_VK_CALL(vkCreateEvent, device, &ci, nullptr, &handle);
	return handle;
}

VkCommandPool createVkCommandPool(VkDevice device, VQueueFamilyId queue_family_id,
								  VCommandPoolFlags flags) {
	VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	poolInfo.flags = toVk(flags);
	poolInfo.queueFamilyIndex = queue_family_id;
	VkCommandPool handle;
	FWK_VK_CALL(vkCreateCommandPool, device, &poolInfo, nullptr, &handle);
	return handle;
}

VkDescriptorPool createVkDescriptorPool(VkDevice device, EnumMap<VDescriptorType, uint> type_counts,
										uint set_count, VDescriptorPoolFlags flags) {
	array<VkDescriptorPoolSize, count<VDescriptorType>> sizes;
	uint num_sizes = 0;
	for(auto type : all<VDescriptorType>)
		if(type_counts[type] > 0)
			sizes[num_sizes++] = {toVk(type), type_counts[type]};

	VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	ci.poolSizeCount = num_sizes;
	ci.pPoolSizes = sizes.data();
	ci.maxSets = set_count;
	ci.flags = flags.bits;
	VkDescriptorPool handle;
	FWK_VK_CALL(vkCreateDescriptorPool, device, &ci, nullptr, &handle);
	return handle;
}

string translateVkResult(VkResult result) {
	const char *stringized = "VK_UNKNOWN";
#define CASE(element)                                                                              \
	case element:                                                                                  \
		stringized = #element;                                                                     \
		break;

	switch(result) {
		CASE(VK_SUCCESS)
		CASE(VK_NOT_READY)
		CASE(VK_TIMEOUT)
		CASE(VK_EVENT_SET)
		CASE(VK_EVENT_RESET)
		CASE(VK_INCOMPLETE)
		CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
		CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
		CASE(VK_ERROR_INITIALIZATION_FAILED)
		CASE(VK_ERROR_DEVICE_LOST)
		CASE(VK_ERROR_MEMORY_MAP_FAILED)
		CASE(VK_ERROR_LAYER_NOT_PRESENT)
		CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
		CASE(VK_ERROR_FEATURE_NOT_PRESENT)
		CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
		CASE(VK_ERROR_TOO_MANY_OBJECTS)
		CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
		CASE(VK_ERROR_FRAGMENTED_POOL)
		CASE(VK_ERROR_UNKNOWN)
		CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
		CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
		CASE(VK_ERROR_FRAGMENTATION)
		CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
		CASE(VK_PIPELINE_COMPILE_REQUIRED)
		CASE(VK_ERROR_SURFACE_LOST_KHR)
		CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
		CASE(VK_SUBOPTIMAL_KHR)
		CASE(VK_ERROR_OUT_OF_DATE_KHR)
		CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
		CASE(VK_ERROR_VALIDATION_FAILED_EXT)
		CASE(VK_ERROR_INVALID_SHADER_NV)
		CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
		CASE(VK_ERROR_NOT_PERMITTED_KHR)
		CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
		CASE(VK_THREAD_IDLE_KHR)
		CASE(VK_THREAD_DONE_KHR)
		CASE(VK_OPERATION_DEFERRED_KHR)
		CASE(VK_OPERATION_NOT_DEFERRED_KHR)
	default:
		break;
	}
#undef CASE

	return stdFormat("%s (0x%x)", stringized, result);
}

static string makeVkMessage(const char *function_name, VkResult result) {
	return format("% % %", function_name, result < 0 ? "failed with" : "returned",
				  translateVkResult(result));
}

Error makeVkError(const char *file, int line, VkResult result, const char *function_name) {
	auto message = makeVkMessage(function_name, result);
	detail::AssertInfo info{};
	info.file = file;
	info.line = line;
	info.message = message.c_str();
	return detail::makeError(&info);
}

void fatalVkError(const char *file, int line, VkResult result, const char *function_name) {
	auto message = makeVkMessage(function_name, result);
	fwk::fatalError(file, line, "%s", message.c_str());
}
}
