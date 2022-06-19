// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H

#include "fwk/sys/platform.h"

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/gfx_base.h"
#include "fwk/vector.h"
#include <vulkan/vulkan.h>

namespace fwk {

// TODO: consistent naming of Vulkan classes:
// VulkanClass: manages lifetime of given object type
// VulkanClassConfig: contains information required for creation of VulkanClass
// VulkanClassInfo: contains all the info for given class, does not manage lifetime
//                  (it's managed elswhere)

DEFINE_ENUM(VVendor, intel, nvidia, amd, other);
DEFINE_ENUM(VFeature, vertex_array_object, debug, copy_image, separate_shader_objects,
			shader_draw_parameters, shader_ballot, shader_subgroup, texture_view, texture_storage,
			texture_s3tc, texture_filter_anisotropic, timer_query);
using VFeatures = EnumFlags<VFeature>;

// TODO: rename to GlMax
DEFINE_ENUM(VLimit, max_elements_indices, max_elements_vertices, max_uniform_block_size,
			max_texture_size, max_texture_3d_size, max_texture_buffer_size, max_texture_anisotropy,
			max_texture_units, max_uniform_locations, max_ssbo_bindings, max_compute_ssbo,
			max_compute_work_group_invocations, max_samples);

DEFINE_ENUM(VDebugLevel, verbose, info, warning, error);
DEFINE_ENUM(VDebugType, general, validation, performance);

DEFINE_ENUM(VQueueFlag, compute, graphics);
using VQueueFlags = EnumFlags<VQueueFlag>;

using VDebugLevels = EnumFlags<VDebugLevel>;
using VDebugTypes = EnumFlags<VDebugType>;

struct VulkanVersion {
	int major, minor, patch;
};

struct VulkanInstanceConfig {
	vector<string> extensions;
	vector<string> layers;

	VulkanVersion version = {1, 2, 0};
	VDebugTypes debug_types = none;
	VDebugLevels debug_levels = none;
};

class VulkanDevice;

struct VulkanSwapChainInfo {
	VkSurfaceCapabilitiesKHR caps;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VulkanPhysicalDeviceInfo {
	// Returned queues are ordered in all find functions
	vector<uint> findQueues(VQueueFlags) const;
	vector<uint> findPresentableQueues(VkSurfaceKHR) const;

	VulkanSwapChainInfo swapChainInfo(VkSurfaceKHR) const;
	double defaultScore() const;

	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties properties;
	vector<VkQueueFamilyProperties> queue_families;
	vector<string> extensions;
};

struct VulkanDeviceConfig {
	VkPhysicalDevice phys_device;
	vector<string> extensions;

	struct QueueConfig {
		uint family_id;
		int count;
	};
	vector<QueueConfig> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

// Only single VulkanInstance can be created
class VulkanInstance {
  public:
	VulkanInstance();
	~VulkanInstance();
	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;

	static vector<string> availableExtensions();
	static vector<string> availableLayers();

	static VulkanInstance *instance();
	Ex<void> initialize(VulkanInstanceConfig);

	VulkanPhysicalDeviceInfo physicalDeviceInfo(VkPhysicalDevice) const;
	vector<VulkanPhysicalDeviceInfo> physicalDeviceInfos() const;

	Maybe<VulkanDeviceConfig> preferredDevice(VkSurfaceKHR target_surface) const;
	Ex<VulkanDevice> makeDevice(const VulkanDeviceConfig &);

	VkInstance handle() { return m_handle; }

  private:
	VkInstance m_handle;
	VkDebugUtilsMessengerEXT m_messenger;
};

}

#endif
