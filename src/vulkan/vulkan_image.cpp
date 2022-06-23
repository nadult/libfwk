// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_ptr.h"
#include "vulkan/vulkan.h"

namespace fwk {

Ex<VPtr<VkBuffer>> VulkanImage::make(VDeviceId device_id, int2 size) {
	auto &vulkan = VulkanInstance::instance();
	auto device = vulkan[device_id].handle;
	return ERROR("finishme");
}
}