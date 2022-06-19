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
#include "fwk/gfx/vulkan_device.h"
#include "fwk/index_range.h"
#include "fwk/parse.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <cstring>

// TODO: handling VkResults
// TODO: making sure that handles are correct ?
// TODO: more type safety
// TODO: VHandle<> class ? handles are not initialized to null by default ...
// TODO: kolejnoœæ niszczenia obiektów...
//
// VulkanDevice jest niszczone przed VulkanWindow, które zawiera SwapChainy ...
// mo¿e po prostu zróbmy ref-county dla du¿ych obiektów ?
// SwapChain bêdzie mia³ ref-count do device-a . Device zostanie zniszczony dopiero jak
// zniszczymy swapchain ?
//
// Mo¿e po prostu zróbmy jeden mechanizm do ref-countów i niszczenia vulkannowych obiektów ?
// Takze dla du¿ych obiektów ?
//
//
//
// Lepiej, ¿eby du¿e klasy siê nie rusza³y, i tak to nie jest do niczego potrzebne.
//
// PVulkanInstance, PVulkanWindow, PVulkanDevice ?
//
//  Czy te wrappery s¹ konieczne do u¿ywania ?
//
// Problemem s¹ klasy które ownuj¹ handle
// Mo¿e po protu powinienem zrobiæ klasê handle-a i u¿ywaæ jej wewn. ?
//
// Olejmy tê kwestiê na razie! Dopoki nie bede mial roznych przykladow uzycia, to
// nie zrobie tego dobrze
//

namespace fwk {

void reportSDLError(const char *func_name) { FATAL("Error on %s: %s", func_name, SDL_GetError()); }

vector<uint> VulkanPhysicalDeviceInfo::findQueues(VQueueFlags flags) const {
	vector<uint> out;
	out.reserve(queue_families.size());
	for(int idx : intRange(queue_families)) {
		auto &queue = queue_families[idx];
		if((flags & VQueueFlag::compute) && !(queue.queueFlags & VK_QUEUE_COMPUTE_BIT))
			continue;
		if((flags & VQueueFlag::graphics) && !(queue.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			continue;
		out.emplace_back(uint(idx));
	}
	return out;
}

vector<uint> VulkanPhysicalDeviceInfo::findPresentableQueues(VkSurfaceKHR surface) const {
	vector<uint> out;
	out.reserve(queue_families.size());
	for(int idx : intRange(queue_families)) {
		VkBool32 valid = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(handle, idx, surface, &valid);
		if(valid == VK_TRUE)
			out.emplace_back(uint(idx));
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

	return {};
}

VulkanPhysicalDeviceInfo VulkanInstance::physicalDeviceInfo(VkPhysicalDevice handle) const {
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

vector<VulkanPhysicalDeviceInfo> VulkanInstance::physicalDeviceInfos() const {
	vector<VulkanPhysicalDeviceInfo> out;
	uint count = 0;
	vkEnumeratePhysicalDevices(m_handle, &count, nullptr);
	vector<VkPhysicalDevice> handles(count);
	vkEnumeratePhysicalDevices(m_handle, &count, handles.data());

	out.reserve(handles.size());
	for(auto handle : handles)
		out.emplace_back(physicalDeviceInfo(handle));
	return out;
}

Maybe<VulkanDeviceConfig> VulkanInstance::preferredDevice(VkSurfaceKHR target_surface) const {
	auto infos = physicalDeviceInfos();

	Maybe<VulkanDeviceConfig> best;
	double best_score = -1.0;

	for(auto &info : infos) {
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
		VulkanDeviceConfig config;
		config.phys_device = info.handle;
		for(auto queue : sel_queues)
			config.queues.emplace_back(queue, 1);
		best = move(config);
	}

	return best;
}

Ex<VulkanDevice> VulkanInstance::makeDevice(const VulkanDeviceConfig &config) {
	ASSERT(config.phys_device);
	EXPECT(!config.queues.empty());

	vector<VkDeviceQueueCreateInfo> queue_cis;
	queue_cis.reserve(config.queues.size());

	float default_priority = 1.0;
	for(auto &queue : config.queues) {
		VkDeviceQueueCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		ci.queueCount = queue.count;
		ci.queueFamilyIndex = queue.family_id;
		ci.pQueuePriorities = &default_priority;
		queue_cis.emplace_back(ci);
	}

	auto swap_chain_ext = "VK_KHR_swapchain";
	vector<const char *> exts = transform(config.extensions, [](auto &str) { return str.c_str(); });
	if(!anyOf(config.extensions, swap_chain_ext))
		exts.emplace_back("VK_KHR_swapchain");

	VkPhysicalDeviceFeatures features{};
	if(config.features)
		features = *config.features;

	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pQueueCreateInfos = queue_cis.data();
	ci.queueCreateInfoCount = queue_cis.size();
	ci.pEnabledFeatures = &features;
	ci.enabledExtensionCount = exts.size();
	ci.ppEnabledExtensionNames = exts.data();

	VkDevice device;
	auto result = vkCreateDevice(config.phys_device, &ci, nullptr, &device);
	if(result != VK_SUCCESS)
		return ERROR("Error during vkCreateDevice: 0x%x", stdFormat("%x", result));

	vector<VkQueue> queues;
	queues.reserve(config.queues.size());
	for(auto &queue_def : config.queues) {
		for(int i : intRange(queue_def.count)) {
			VkQueue queue = nullptr;
			vkGetDeviceQueue(device, queue_def.family_id, i, &queue);
			queues.emplace_back(queue);
		}
	}

	return VulkanDevice(device, move(queues), physicalDeviceInfo(config.phys_device));
}
}
