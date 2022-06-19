// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifdef FWK_GFX_VULKAN_H
#define FWK_GFX_VULKAN_H_ONLY_EXTENSIONS
#undef FWK_GFX_VULKAN_H
#endif

#include "fwk/vulkan/vulkan_instance.h"

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

vector<VQueueFamilyId> VulkanPhysicalDeviceInfo::findQueues(VQueueFlags flags) const {
	vector<VQueueFamilyId> out;
	out.reserve(queue_families.size());
	for(int idx : intRange(queue_families)) {
		auto &queue = queue_families[idx];
		if((flags & VQueueFlag::compute) && !(queue.queueFlags & VK_QUEUE_COMPUTE_BIT))
			continue;
		if((flags & VQueueFlag::graphics) && !(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			continue;
		out.emplace_back(idx);
	}
	return out;
}

vector<VQueueFamilyId> VulkanPhysicalDeviceInfo::findPresentableQueues(VkSurfaceKHR surface) const {
	vector<VQueueFamilyId> out;
	out.reserve(queue_families.size());
	for(int idx : intRange(queue_families)) {
		VkBool32 valid = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(handle, idx, surface, &valid);
		if(valid == VK_TRUE)
			out.emplace_back(idx);
	}
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

VulkanSwapChainInfo VulkanPhysicalDeviceInfo::swapChainInfo(VkSurfaceKHR surface) const {
	VulkanSwapChainInfo out;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(handle, surface, &out.caps);
	uint count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(handle, surface, &count, nullptr);
	out.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(handle, surface, &count, out.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(handle, surface, &count, nullptr);
	out.present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(handle, surface, &count, out.present_modes.data());
	return out;
}

double VulkanPhysicalDeviceInfo::defaultScore() const {
	double score = 0.0;
	if(findQueues(VQueueFlag::graphics))
		score += 1000.0;
	if(findQueues(VQueueFlag::compute))
		score += 100.0;
	if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 10.0;
	else if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		score += 5.0;
	return score;
}

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

static VulkanInstance *s_instance = nullptr;
bool VulkanInstance::isPresent() { return !!s_instance; }
VulkanInstance &VulkanInstance::instance() {
	PASSERT(s_instance);
	return *s_instance;
}

VulkanInstance::VulkanInstance() : m_handle(0), m_messenger(0) {
	ASSERT("Only one instance of VulkanInstance can be created at a time" && !s_instance);
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_InitSubSystem");
	s_instance = this;
}
VulkanInstance::~VulkanInstance() {
	s_instance = nullptr;
	for(auto &device : m_devices)
		vkDestroyDevice(device.handle, nullptr);
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

static VulkanPhysicalDeviceInfo physicalDeviceInfo(VkPhysicalDevice handle) {
	VulkanPhysicalDeviceInfo out;
	out.handle = handle;

	uint count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
	out.queue_families.resize(count);
	vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, out.queue_families.data());

	count = 0;
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, nullptr);
	vector<VkExtensionProperties> exts(count);
	vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, exts.data());
	out.extensions = transform(exts, [](auto &prop) -> string { return prop.extensionName; });

	vkGetPhysicalDeviceProperties(handle, &out.properties);

	return out;
}

static vector<VulkanPhysicalDeviceInfo> physicalDeviceInfos(VkInstance instance) {
	vector<VulkanPhysicalDeviceInfo> out;
	uint count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	vector<VkPhysicalDevice> handles(count);
	vkEnumeratePhysicalDevices(instance, &count, handles.data());

	out.reserve(handles.size());
	for(auto handle : handles) {
		out.emplace_back(physicalDeviceInfo(handle));
		return out;
	}
}

Ex<void> VulkanInstance::initialize(const VulkanInstanceSetup &setup) {
	if(m_handle)
		return ERROR("VulkanInstance already initialized");

	bool enable_validation = setup.debug_levels && setup.debug_types;

	auto extensions = setup.extensions;
	auto layers = setup.layers;

	if(enable_validation) {
		auto debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		auto debug_layer = "VK_LAYER_KHRONOS_validation";
		extensions.emplace_back(debug_ext);
		layers.emplace_back(debug_layer);
	}

	insertBack(extensions, vulkanSurfaceExtensions());
	makeSortedUnique(extensions);
	makeSortedUnique(layers);

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "fwk";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion =
		VK_MAKE_API_VERSION(0, setup.version.major, setup.version.minor, setup.version.patch);

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	auto ext_names = transform(extensions, [](const string &str) { return str.c_str(); });
	auto layer_names = transform(layers, [](const string &str) { return str.c_str(); });

	create_info.enabledExtensionCount = ext_names.size();
	create_info.ppEnabledExtensionNames = ext_names.data();
	create_info.enabledLayerCount = layer_names.size();
	create_info.ppEnabledLayerNames = layer_names.data();

	VkResult result = vkCreateInstance(&create_info, nullptr, &m_handle);
	if(result != VK_SUCCESS)
		return ERROR("Error on vkCreateInstance: 0x%x", uint(result));

	if(enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT create_info{};
		auto dtypes = setup.debug_types;
		auto dlevels = setup.debug_levels;
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

	m_phys_devices = physicalDeviceInfos(m_handle);
	return {};
}

bool VulkanInstance::valid(VDeviceId id) const { return m_devices.valid(id); }
bool VulkanInstance::valid(VPhysicalDeviceId id) const { return m_phys_devices.inRange(id); }

const VulkanDeviceInfo &VulkanInstance::operator[](VDeviceId id) const { return m_devices[id]; }
const VulkanPhysicalDeviceInfo &VulkanInstance::operator[](VPhysicalDeviceId id) const {
	return m_phys_devices[id];
}

vector<VDeviceId> VulkanInstance::deviceIds() const {
	vector<VDeviceId> out;
	out.reserve(m_devices.size());
	for(auto idx : m_devices.indices())
		out.emplace_back(idx);
	return out;
}

SimpleIndexRange<VPhysicalDeviceId> VulkanInstance::physicalDeviceIds() const {
	return indexRange<VPhysicalDeviceId>(m_phys_devices.size());
}

Maybe<VPhysicalDeviceId>
VulkanInstance::preferredDevice(VkSurfaceKHR target_surface,
								vector<VulkanQueueSetup> *out_queues) const {
	Maybe<VPhysicalDeviceId> best;
	double best_score = -1.0;

	for(auto id : physicalDeviceIds()) {
		auto &info = m_phys_devices[id];
		double score = info.defaultScore();
		if(score <= best_score)
			continue;

		auto gfx_queues = info.findQueues(VQueueFlag::compute | VQueueFlag::graphics);
		if(!gfx_queues)
			gfx_queues = info.findQueues(VQueueFlag::graphics);
		auto present_queues = info.findPresentableQueues(target_surface);
		if(!gfx_queues || !present_queues)
			continue;

		auto sel_queues = setIntersection(gfx_queues, present_queues);
		if(!sel_queues) {
			sel_queues.emplace_back(gfx_queues[0]);
			sel_queues.emplace_back(present_queues[0]);
		}

		best_score = score;
		best = id;

		if(out_queues) {
			vector<VulkanQueueSetup> queue_setup;
			for(auto queue : sel_queues)
				queue_setup.emplace_back(queue, 1);
			*out_queues = move(queue_setup);
		}
	}

	return best;
}

Ex<VDeviceId> VulkanInstance::createDevice(VPhysicalDeviceId phys_id,
										   const VulkanDeviceSetup &setup) {
	ASSERT(valid(phys_id));
	EXPECT(!setup.queues.empty());

	vector<VkDeviceQueueCreateInfo> queue_cis;
	queue_cis.reserve(setup.queues.size());

	float default_priority = 1.0;
	for(auto &queue : setup.queues) {
		VkDeviceQueueCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		ci.queueCount = queue.count;
		ci.queueFamilyIndex = queue.family_id;
		ci.pQueuePriorities = &default_priority;
		queue_cis.emplace_back(ci);
	}

	auto swap_chain_ext = "VK_KHR_swapchain";
	vector<const char *> exts = transform(setup.extensions, [](auto &str) { return str.c_str(); });
	if(!anyOf(setup.extensions, swap_chain_ext))
		exts.emplace_back("VK_KHR_swapchain");

	VkPhysicalDeviceFeatures features{};
	if(setup.features)
		features = *setup.features;

	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = exts.size();
	ci.ppEnabledExtensionNames = exts.data();

	VkDevice device;
	auto phys_handle = m_phys_devices[phys_id].handle;
	auto result = vkCreateDevice(phys_handle, &ci, nullptr, &device);
	if(result != VK_SUCCESS)
		return ERROR("Error during vkCreateDevice: 0x%x", stdFormat("%x", result));

	vector<VkQueue> queues;
	queues.reserve(setup.queues.size());
	for(auto &queue_def : setup.queues) {
		for(int i : intRange(queue_def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(device, queue_def.family_id, i, &queue);
			queues.emplace_back(queue);
		}
	}

	auto id = m_devices.emplace(device, phys_id, move(queues));
	return VDeviceId(id);
}

void VulkanInstance::destroyDevice(VDeviceId id) {
	auto &info = m_devices[id];
	vkDestroyDevice(info.handle, nullptr);
	m_devices.erase(id);
}
}
