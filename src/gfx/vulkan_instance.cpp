// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifdef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H_ONLY_EXTENSIONS
#undef FWK_GFX_VULKAN_H
#endif

#include "fwk/gfx/vulkan_instance.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/color.h"
#include "fwk/index_range.h"
#include "fwk/parse.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <cstring>

namespace fwk {

void reportSDLError(const char *func_name) { FATAL("Error on %s: %s", func_name, SDL_GetError()); }

vector<string> vulkanSurfaceExtensions() {
	vector<string> out;
	out.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef FWK_PLATFORM_WINDOWS
	out.emplace_back("VK_KHR_win32_surface");
#elif defined(FWK_PLATFORM_LINUX)
	if(std::getenv("WAYLAND_DISPLAY"))
		out.emplace_back("VK_KHR_wayland_surface");
	else
		out.emplace_back("VK_KHR_xlib_surface");
}
#endif
	return out;
}

vector<string> VulkanInstance::availableExtensions() {
	uint num_extensions = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, nullptr);
	vector<VkExtensionProperties> extensions(num_extensions);
	vkEnumerateInstanceExtensionProperties(nullptr, &num_extensions, extensions.data());
	auto out = transform(extensions, [](const auto &prop) -> string { return prop.extensionName; });
	makeSorted(out);
	return out;
}

vector<string> VulkanInstance::availableLayers() {
	uint num_layers = 0;
	vkEnumerateInstanceLayerProperties(&num_layers, nullptr);
	vector<VkLayerProperties> layers(num_layers);
	vkEnumerateInstanceLayerProperties(&num_layers, layers.data());
	auto out = transform(layers, [](auto &prop) -> string { return prop.layerName; });
	makeSorted(out);
	return out;
}

static VulkanInstance *s_instance = nullptr;
VulkanInstance *VulkanInstance::instance() { return s_instance; }

VulkanInstance::VulkanInstance() : m_handle(0), m_messenger(0) {
	ASSERT("Only one instance of VulkanInstance can be created at a time" && !s_instance);
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_InitSubSystem");
	s_instance = this;
}
VulkanInstance::~VulkanInstance() {
	s_instance = nullptr;
	if(m_messenger) {
		if(auto destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			   m_handle, "vkDestroyDebugUtilsMessengerEXT"))
			destroy_func(m_handle, m_messenger, nullptr);
	}
	if(m_handle)
		vkDestroyInstance(m_handle, nullptr);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
messageHandler(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
			   VkDebugUtilsMessageTypeFlagsEXT message_type,
			   const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
	print("Vulkan debug: %\n", callback_data->pMessage);
	return VK_FALSE;
}

Ex<void> VulkanInstance::initialize(VulkanInstanceConfig config) {
	if(m_handle)
		return ERROR("VulkanInstance already initialized");

	bool enable_validation = config.debug_levels && config.debug_types;

	if(enable_validation) {
		auto debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		auto debug_layer = "VK_LAYER_KHRONOS_validation";
		config.extensions.emplace_back(debug_ext);
		config.layers.emplace_back(debug_layer);
	}

	insertBack(config.extensions, vulkanSurfaceExtensions());
	makeSortedUnique(config.extensions);
	makeSortedUnique(config.layers);

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "fwk";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion =
		VK_MAKE_API_VERSION(0, config.version.major, config.version.minor, config.version.patch);

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	auto ext_names = transform(config.extensions, [](const string &str) { return str.c_str(); });
	auto layer_names = transform(config.layers, [](const string &str) { return str.c_str(); });

	create_info.enabledExtensionCount = ext_names.size();
	create_info.ppEnabledExtensionNames = ext_names.data();
	create_info.enabledLayerCount = layer_names.size();
	create_info.ppEnabledLayerNames = layer_names.data();

	VkResult result = vkCreateInstance(&create_info, nullptr, &m_handle);
	if(result != VK_SUCCESS)
		return ERROR("Error on vkCreateInstance: 0x%x", uint(result));

	if(enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT create_info{};
		auto dtypes = config.debug_types;
		auto dlevels = config.debug_levels;
		using DType = VDebugType;
		using DLevel = VDebugLevel;

		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity =
			(dlevels & DLevel::verbose ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : 0) |
			(dlevels & DLevel::info ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : 0) |
			(dlevels & DLevel::warning ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT : 0) |
			(dlevels & DLevel::error ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT : 0);

		create_info.messageType =
			(dtypes & DType::general ? VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT : 0) |
			(dtypes & DType::validation ? VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : 0) |
			(dtypes & DType::performance ? VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT : 0);
		create_info.pfnUserCallback = messageHandler;
		create_info.pUserData = nullptr;

		auto hook_messenger_func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			m_handle, "vkCreateDebugUtilsMessengerEXT");
		if(!hook_messenger_func)
			return ERROR("Cannot hook vulkan debug message handler");
		result = hook_messenger_func(m_handle, &create_info, nullptr, &m_messenger);
		if(result != VK_SUCCESS)
			return ERROR("Error while hooking vulkan debug message handler: 0x%x", uint(result));
	}

	uint phys_device_count = 0;
	vkEnumeratePhysicalDevices(m_handle, &phys_device_count, nullptr);
	m_phys_devices.resize(phys_device_count);
	vkEnumeratePhysicalDevices(m_handle, &phys_device_count, m_phys_devices.data());

	return {};
}

vector<VkQueueFamilyProperties> VulkanInstance::deviceQueueFamilies(VkPhysicalDevice device) const {
	uint count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
	vector<VkQueueFamilyProperties> out(count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, out.data());
	return out;
}

vector<string> VulkanInstance::deviceExtensions(VkPhysicalDevice device) const {
	uint ext_count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);
	vector<VkExtensionProperties> exts(ext_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, exts.data());
	return transform(exts, [](auto &prop) -> string { return prop.extensionName; });
}

VkPhysicalDeviceProperties VulkanInstance::deviceProperties(VkPhysicalDevice device) const {
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(device, &props);
	return props;
}

using SwapChainInfo = VulkanInstance::SwapChainInfo;
SwapChainInfo VulkanInstance::swapChainInfo(VkPhysicalDevice device, VkSurfaceKHR surface) const {
	SwapChainInfo out;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out.caps);
	uint count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
	out.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, out.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
	out.present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, out.present_modes.data());
	return out;
}

Maybe<VkPhysicalDevice> VulkanInstance::preferredPhysicalDevice() const {
	Maybe<VkPhysicalDevice> best;
	float best_score = -1;

	// TODO: check for swap_chain and presentability
	for(auto device : m_phys_devices) {
		auto props = deviceProperties(device);
		auto queues = deviceQueueFamilies(device);

		bool has_graphics = false;
		bool has_compute = false;
		for(auto &queue : queues) {
			if(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				has_graphics = true;
			if(queue.queueFlags & VK_QUEUE_COMPUTE_BIT)
				has_compute = true;
		}

		if(!has_graphics)
			continue;

		float score = 0.0;
		if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score += 1000.0;
		else if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score += 100.0;
		if(has_compute)
			score += 500.0;

		if(score > best_score) {
			best = device;
			best_score = score;
		}
	}

	return best;
}

/*
static EnumMap<VkLimit, int> s_limit_map = {
	{GL_MAX_ELEMENTS_INDICES, GL_MAX_ELEMENTS_VERTICES, GL_MAX_UNIFORM_BLOCK_SIZE,
	 GL_MAX_TEXTURE_SIZE, GL_MAX_3D_TEXTURE_SIZE, GL_MAX_TEXTURE_BUFFER_SIZE,
	 GL_MAX_TEXTURE_MAX_ANISOTROPY, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GL_MAX_UNIFORM_LOCATIONS,
	 GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,
	 GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, GL_MAX_SAMPLES}};*/

/*
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

	int major = 0, minor = 0;
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
	s_info.extensions.clear();


	for(auto limit : all<VkLimit>)
		glGetIntegerv(s_limit_map[limit], &s_info.limits[limit]);
	{
		auto *ver = (const char *)glGetString(GL_VERSION);
		auto *glsl_ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

		s_info.version_full = ver;
		s_info.glsl_version_full = glsl_ver;
		s_info.glsl_version = parseOpenglVersion(glsl_ver);
		// TODO: multiple GLSL versions can be supported
	}

// TODO: limits
// TODO: features
}*/

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
