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

	Maybe<uint> findMemoryType(u32 type_bits, VkMemoryPropertyFlags) const;
	double defaultScore() const;

	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties mem_properties;
	vector<VkQueueFamilyProperties> queue_families;
	vector<string> extensions;
};

struct VulkanQueueSetup;
struct VulkanDeviceSetup;

struct VResourceId {
	static constexpr int max_vulkan_devices = 16;
	static constexpr int max_resource_types = 16;
	static_assert(count<VTypeId> <= max_resource_types);

	VResourceId(VTypeId type_id, VDeviceId device_id, uint object_id)
		: bits(object_id | (uint(device_id) << 24) | (uint(type_id) << 28)) {}

	VResourceId() : bits(0) {}

	uint bits;
};

struct VDownloadId {
	uint res_id = 0;
};

struct VulkanUploadImageOp {
	VResourceId image_id;
	VkFormat format;
	PodVector<u8> data;
};

struct VulkanUploadBufferOp {
	VResourceId buffer_id;
	PodVector<u8> data;
	uint offset = 0;
};

struct VulkanDownloadBufferOp {
	VResourceId buffer_id;
	VDownloadId download_buffer_id;
	uint offset = 0, size = 0;
};

struct VulkanDownloadImageOp {
	VResourceId buffer_id;
	VDownloadId download_image_id;
	uint offset = 0, size = 0;
};

struct VulkanRenderOp {
	VResourceId render_pass_id;
	// TODO: ?
};

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
											 vector<VulkanQueueSetup> * = nullptr) const;
	Ex<VDeviceRef> createDevice(VPhysicalDeviceId, const VulkanDeviceSetup &);

	VkInstance handle() { return m_handle; }

  private:
	friend class VulkanStorage;

	VulkanInstance();
	~VulkanInstance();

	Ex<void> initialize(const VulkanInstanceSetup &);

	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;

	VkInstance m_handle = nullptr;
	VkDebugUtilsMessengerEXT m_messenger = nullptr;
	vector<VulkanPhysicalDeviceInfo> m_phys_devices;
};

}

#endif
