// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/tag_id.h"

struct VkSurfaceKHR_T;
using VkSurfaceKHR = VkSurfaceKHR_T *;

namespace fwk {

DEFINE_ENUM(VTag, device, physical_device, queue_family);

using VDeviceId = TagId<VTag::device, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;

struct VulkanVersion {
	int major, minor, patch;
};

class VulkanInstance;
class VulkanWindow;

using VInstance = VulkanInstance;
using VWindow = VulkanWindow;

}
