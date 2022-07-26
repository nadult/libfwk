// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/platform.h"

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/gfx_base.h"
#include "fwk/index_range.h"
#include "fwk/sparse_vector.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

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
	vector<VQueueFamilyId> findQueues(VQueueCaps) const;
	vector<VQueueFamilyId> findPresentableQueues(VkSurfaceKHR) const;

	int findMemoryType(u32 type_bits, VMemoryFlags) const;
	double defaultScore() const;

	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties mem_properties;
	vector<VkQueueFamilyProperties> queue_families;
	vector<string> extensions;
	VDepthStencilFormats supported_depth_stencil_formats;
	VBlockFormats supported_block_color_formats;
	vector<VkFormat> supported_color_formats;
	vector<VkFormat> supported_color_attachment_formats;
};

struct VQueueSetup;
struct VDeviceSetup;

// Singleton class for Vulkan instance object
class VulkanInstance {
  public:
	static vector<string> availableExtensions();
	static vector<string> availableLayers();

	static bool isPresent();
	static VInstanceRef ref();
	static Ex<VInstanceRef> create(const VulkanInstanceSetup &);

	bool valid(VPhysicalDeviceId) const;
	const VulkanPhysicalDeviceInfo &info(VPhysicalDeviceId) const;
	SimpleIndexRange<VPhysicalDeviceId> physicalDeviceIds() const;

	Maybe<VPhysicalDeviceId> preferredDevice(VkSurfaceKHR target_surface,
											 vector<VQueueSetup> * = nullptr) const;
	Ex<VDeviceRef> createDevice(VPhysicalDeviceId, const VDeviceSetup &);

	VkInstance handle() { return m_handle; }
	auto version() const { return m_version; }

  private:
	friend class VulkanStorage;

	VulkanInstance();
	~VulkanInstance();

	Ex<void> initialize(const VulkanInstanceSetup &);

	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;

	VulkanVersion m_version;
	VkInstance m_handle = nullptr;
	VkDebugUtilsMessengerEXT m_messenger = nullptr;
	vector<VulkanPhysicalDeviceInfo> m_phys_devices;
};
}
