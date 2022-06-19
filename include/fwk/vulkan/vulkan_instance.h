// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H

#include "fwk/sys/platform.h"

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/gfx_base.h"
#include "fwk/index_range.h"
#include "fwk/sparse_vector.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"
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

DEFINE_ENUM(VDebugLevel, verbose, info, warning, error);
DEFINE_ENUM(VDebugType, general, validation, performance);

DEFINE_ENUM(VQueueFlag, compute, graphics);
using VQueueFlags = EnumFlags<VQueueFlag>;

using VDebugLevels = EnumFlags<VDebugLevel>;
using VDebugTypes = EnumFlags<VDebugType>;

struct VulkanInstanceSetup {
	vector<string> extensions;
	vector<string> layers;

	VulkanVersion version = {1, 2, 0};
	VDebugTypes debug_types = none;
	VDebugLevels debug_levels = none;
};

struct VulkanSwapChainInfo {
	VkSurfaceCapabilitiesKHR caps;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VulkanPhysicalDeviceInfo {
	// Returned queues are ordered in all find functions
	vector<VQueueFamilyId> findQueues(VQueueFlags) const;
	vector<VQueueFamilyId> findPresentableQueues(VkSurfaceKHR) const;

	VulkanSwapChainInfo swapChainInfo(VkSurfaceKHR) const;
	double defaultScore() const;

	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties properties;
	vector<VkQueueFamilyProperties> queue_families;
	vector<string> extensions;
};

struct VulkanQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VulkanDeviceSetup {
	vector<string> extensions;
	vector<VulkanQueueSetup> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

struct VulkanDeviceInfo {
	VkDevice handle;
	VPhysicalDeviceId physical_device_id;
	vector<VkQueue> queues;
};

// Only single VulkanInstance can be created
class VulkanInstance {
  public:
	using DeviceInfo = VulkanDeviceInfo;
	using PhysicalDeviceInfo = VulkanPhysicalDeviceInfo;

	VulkanInstance();
	~VulkanInstance();
	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;

	static vector<string> availableExtensions();
	static vector<string> availableLayers();

	static bool isPresent();
	static VulkanInstance &instance();
	Ex<void> initialize(const VulkanInstanceSetup &);

	bool valid(VDeviceId) const;
	bool valid(VPhysicalDeviceId) const;

	const DeviceInfo &operator[](VDeviceId id) const;
	const PhysicalDeviceInfo &operator[](VPhysicalDeviceId id) const;

	vector<VDeviceId> deviceIds() const;
	SimpleIndexRange<VPhysicalDeviceId> physicalDeviceIds() const;

	Maybe<VPhysicalDeviceId> preferredDevice(VkSurfaceKHR target_surface,
											 vector<VulkanQueueSetup> * = nullptr) const;
	Ex<VDeviceId> createDevice(VPhysicalDeviceId, const VulkanDeviceSetup &);
	void destroyDevice(VDeviceId);

	VkInstance handle() { return m_handle; }

  private:
	VkInstance m_handle;
	vector<PhysicalDeviceInfo> m_phys_devices;
	SparseVector<DeviceInfo> m_devices;
	VkDebugUtilsMessengerEXT m_messenger;
};

}

#endif
