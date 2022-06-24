// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_image.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_storage.h"
#include "vulkan/vulkan.h"

namespace fwk {

Ex<VPtr<VkBuffer>> VulkanImage::make(VDeviceRef device, int2 size) { return ERROR("finishme"); }
}