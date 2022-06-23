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

// TODO: enforce max_vulkan_devices

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

struct VulkanPhysicalDeviceInfo {
	// Returned queues are ordered in all find functions
	vector<VQueueFamilyId> findQueues(VQueueFlags) const;
	vector<VQueueFamilyId> findPresentableQueues(VkSurfaceKHR) const;

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
	// TODO: signify which queue is for what?
	vector<Pair<VkQueue, VQueueFamilyId>> queues;
};

// Singleton class for Vulkan instance object
class VulkanInstance {
  public:
	using DeviceInfo = VulkanDeviceInfo;
	using PhysicalDeviceInfo = VulkanPhysicalDeviceInfo;

	static vector<string> availableExtensions();
	static vector<string> availableLayers();

	static bool isPresent() { return g_instance.m_handle != nullptr; }
	static VulkanInstance &instance() { return PASSERT(isPresent()), g_instance; }

	static Ex<void> create(const VulkanInstanceSetup &);
	// Can be called multiple times, even when create failed
	static void destroy();

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

	void nextReleasePhase();

	VkInstance handle() { return m_handle; }

  private:
	template <class T> friend class VLightPtr;
	template <class T> friend class VWrapPtr;

	VulkanInstance();
	~VulkanInstance();

	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;
	Ex<void> create_(const VulkanInstanceSetup &);
	void destroy_();

	static VulkanInstance g_instance;
	static VulkanObjectManager g_obj_managers[count<VTypeId>];

	VkInstance m_handle = nullptr;
	VkDebugUtilsMessengerEXT m_messenger = nullptr;

	vector<PhysicalDeviceInfo> m_phys_devices;
	SparseVector<DeviceInfo> m_devices;
};

}

#endif
