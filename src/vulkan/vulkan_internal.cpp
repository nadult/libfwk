// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_internal.h"

namespace fwk {

string vulkanTranslateResult(VkResult result) {
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

Error vulkanMakeError(const char *file, int line, VkResult result, const char *function_name) {
	auto message = format("% % %", function_name, result < 0 ? "failed with" : "returned",
						  vulkanTranslateResult(result));
	detail::AssertInfo info{};
	info.file = file;
	info.line = line;
	info.message = message.c_str();
	return detail::makeError(&info);
}
}
