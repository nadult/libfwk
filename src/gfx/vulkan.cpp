// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifdef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H_ONLY_EXTENSIONS
#undef FWK_GFX_VULKAN_H
#endif

#include "fwk/gfx/vulkan.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/color.h"
#include "fwk/parse.h"
#include "fwk/str.h"
#include <cstring>

namespace fwk {

void *winLoadFunction(const char *name);

static VkInfo s_info;
const VkInfo *const vk_info = &s_info;

// TODO: better way to fix it?
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#endif

/*
static EnumMap<VkLimit, int> s_limit_map = {
	{GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_UNIFORM_BLOCK_SIZE,
	 GL_MAX_TEXTURE_SIZE, GL_MAX_3D_TEXTURE_SIZE, GL_MAX_TEXTURE_BUFFER_SIZE,
	 GL_MAX_TEXTURE_MAX_ANISOTROPY, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GL_MAX_UNIFORM_LOCATIONS,
	 GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,
	 GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, GL_MAX_SAMPLES}};*/

string VkInfo::toString() const {
	TextFormatter out;
	out("Vendor: %\nRenderer: %\nExtensions: %\nLayers: %\nVersion: % (%)\nGLSL version: % (%)\n",
		vendor, renderer, extensions, layers, version, version_full, glsl_version,
		glsl_version_full);

	out("Limits:\n");
	for(auto limit : all<VkLimit>)
		out("  %: %\n", limit, limits[limit]);
	out(" *max_compute_work_group_size: %\n *max_compute_work_groups: %\n",
		max_compute_work_group_size, max_compute_work_groups);

	return out.text();
}

bool VkInfo::hasExtension(Str name) const {
	auto it = std::lower_bound(begin(extensions), end(extensions), name,
							   [](Str a, Str b) { return a < b; });

	return it != end(extensions) && Str(*it) == name;
}

static float parseOpenglVersion(const char *text) {
	TextParser parser(text);
	while(!parser.empty()) {
		Str val;
		parser >> val;
		if(isdigit(val[0]))
			return tryFromString<float>(string(val), 0.0f);
	}
	return 0.0f;
}

void initializeVulkan() {
	using Feature = VkFeature;
	using Vendor = VkVendor;

	/*int major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	s_info.version = float(major) + float(minor) * 0.1f;

	auto vendor = toLower((const char *)glGetString(GL_VENDOR));
	if(vendor.find("intel") != string::npos)
		s_info.vendor = Vendor::intel;
	else if(vendor.find("nvidia") != string::npos)
		s_info.vendor = Vendor::nvidia;
	else if(vendor.find("amd") != string::npos)
		s_info.vendor = Vendor::amd;
	else
		s_info.vendor = Vendor::other;
	s_info.renderer = (const char *)glGetString(GL_RENDERER);

	GLint num;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num);
	s_info.extensions.reserve(num);
	s_info.extensions.clear();*/

	uint num_extensions = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);
	vector<VkExtensionProperties> extensions(num_extensions);
	vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, extensions.data());
	s_info.extensions =
		transform(extensions, [](const auto &prop) -> string { return prop.extensionName; });
	makeSorted(s_info.extensions);

	uint num_layers = 0;
	vkEnumerateInstanceLayerProperties(&num_layers, nullptr);
	vector<VkLayerProperties> layers(num_layers);
	vkEnumerateInstanceLayerProperties(&num_layers, layers.data());
	s_info.layers = transform(layers, [](auto &prop) -> string { return prop.layerName; });
	makeSorted(s_info.layers);

	/*for(auto limit : all<VkLimit>)
		glGetIntegerv(s_limit_map[limit], &s_info.limits[limit]);
	{
		auto *ver = (const char *)glGetString(GL_VERSION);
		auto *glsl_ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

		s_info.version_full = ver;
		s_info.glsl_version_full = glsl_ver;
		s_info.glsl_version = parseOpenglVersion(glsl_ver);
		// TODO: multiple GLSL versions can be supported
	}*/

	// TODO: limits
	// TODO: features
}

/*
void clearColor(FColor col) {
	glClearColor(col.r, col.g, col.b, col.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void clearColor(IColor col) { clearColor(FColor(col)); }

void clearDepth(float value) {
	glClearDepth(value);
	glDepthMask(1);
	glClear(GL_DEPTH_BUFFER_BIT);
}*/
}
