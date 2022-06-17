// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H

#include "fwk/sys/platform.h"

#if defined(FWK_PLATFORM_MINGW) || defined(FWK_PLATFORM_MSVC)
#ifndef _WINDOWS_
#define _WINDOWS_
#define APIENTRY __attribute__((__stdcall__))
#define WINGDIAPI __attribute__((dllimport))
#endif
#else
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <vulkan/vulkan.h>

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/gfx_base.h"
#include "fwk/vector.h"

namespace fwk {

void testGlError(const char *);
bool installGlDebugHandler();

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

DEFINE_ENUM(VInstanceFlag, validation);
using VInstanceFlags = EnumFlags<VInstanceFlag>;

struct VulkanInstanceInfo {
	vector<string> extensions;
	vector<string> layers;
};

VulkanInstanceInfo getVulkanInstanceInfo();

struct VulkanVersion {
	int major, minor, patch;
};

struct VulkanInstanceConfig {
	vector<string> extensions;
	vector<string> layers;
	VInstanceFlags flags;
	VulkanVersion version = {1, 2, 0};
};

// Only single VulkanInstance can be created
class VulkanInstance {
  public:
	using Flag = VInstanceFlag;

	VulkanInstance();
	~VulkanInstance();
	VulkanInstance(const VulkanInstance &) = delete;
	void operator=(const VulkanInstance &) = delete;

	static VulkanInstance *instance();
	Ex<void> initialize(VulkanInstanceConfig);

  private:
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_messenger;
};

/*
struct VkInfo {
	VkVendor vendor;

	vector<string> extensions;
	vector<string> layers;

	VkFeatures features;
	EnumMap<VkLimit, int> limits;

	int3 max_compute_work_group_size;
	int3 max_compute_work_groups;

	bool hasExtension(Str) const;
	bool hasFeature(VkFeature feature) const { return (bool)(features & feature); }

	string renderer;
	string version_full;
	string glsl_version_full;

	float version;
	float glsl_version;

	string toString() const;
};*/

void clearColor(FColor);
void clearColor(IColor);
void clearDepth(float);

}

#endif
