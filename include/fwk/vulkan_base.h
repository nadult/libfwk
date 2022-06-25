// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/tag_id.h"

// TODO: try to remove it if possible
#include <vulkan/vulkan.h>

namespace fwk {

DEFINE_ENUM(VTag, device, window, physical_device, queue_family);

using VDeviceId = TagId<VTag::device, u8>;
using VWindowId = TagId<VTag::window, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;

class VObjectId;

struct VulkanVersion {
	int major, minor, patch;
};

DEFINE_ENUM(VTypeId, buffer, command_pool, command_buffer, descriptor_pool, descriptor_set_layout,
			fence, framebuffer, image, image_view, pipeline, pipeline_layout, render_pass, sampler,
			semaphore, shader_module);

class VulkanDevice;
class VulkanInstance;
class VulkanWindow;

class VInstanceRef;
class VDeviceRef;
class VWindowRef;

template <class T> class VPtr;

template <class> struct VulkanTypeInfo;
#define CASE_TYPE(UpperCase, lower_case)                                                           \
	class Vulkan##UpperCase;                                                                       \
	template <> struct VulkanTypeInfo<Vk##UpperCase> {                                             \
		static constexpr VTypeId type_id = VTypeId::lower_case;                                    \
		using Wrapper = Vulkan##UpperCase;                                                         \
	};                                                                                             \
	using PV##UpperCase = VPtr<Vk##UpperCase>;
#include "fwk/vulkan/vulkan_type_list.h"

template <class T>
constexpr bool vk_type_wrapped = !is_same<typename VulkanTypeInfo<T>::Wrapper, None>;

}
