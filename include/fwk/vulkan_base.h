// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/tag_id.h"

#include <vulkan/vulkan.h>

struct VkSurfaceKHR_T;
using VkSurfaceKHR = VkSurfaceKHR_T *;

namespace fwk {

DEFINE_ENUM(VTag, device, window, physical_device, queue_family);

using VDeviceId = TagId<VTag::device, u8>;
using VWindowId = TagId<VTag::window, u8>;
using VPhysicalDeviceId = TagId<VTag::physical_device, u8>;
using VQueueFamilyId = TagId<VTag::queue_family, u8>;

struct VulkanVersion {
	int major, minor, patch;
};

DEFINE_ENUM(VTypeId, buffer, command_pool, command_buffer, descriptor_pool, descriptor_set_layout,
			fence, framebuffer, image, image_view, pipeine, pipeline_layout, render_pass, sampler,
			semaphore, shader_module);

class VulkanDevice;
class VulkanInstance;
class VulkanWindow;

class VInstanceRef;
class VDeviceRef;
class VWindowRef;

class VulkanImage;
class VulkanBuffer;
class VulkanRenderPass;

template <class T> class VLightPtr;
template <class T> class VWrapPtr;

class VulkanObjectManager;

using VInstance = VulkanInstance;
using VWindow = VulkanWindow;

template <class> struct VulkanTypeInfo;

#define CASE_TYPE(Wrapper_, VkType, type_id_)                                                      \
	template <> struct VulkanTypeInfo<VkType> {                                                    \
		static constexpr VTypeId type_id = VTypeId::type_id_;                                      \
		using Wrapper = Wrapper_;                                                                  \
	};
#include "fwk/vulkan/vulkan_types.h"

template <class T>
using VPtr = If<is_same<typename VulkanTypeInfo<T>::Wrapper, None>, VLightPtr<T>, VWrapPtr<T>>;

using PVImage = VPtr<VkImage>;
using PVBuffer = VPtr<VkBuffer>;
using PVSemaphore = VPtr<VkSemaphore>;
using PVFence = VPtr<VkFence>;
using PVShaderModule = VPtr<VkShaderModule>;

}
