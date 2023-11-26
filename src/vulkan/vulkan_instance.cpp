// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_instance.h"

#include "fwk/enum_map.h"
#include "fwk/format.h"
#include "fwk/gfx/color.h"
#include "fwk/index_range.h"
#include "fwk/parse.h"
#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

vector<VQueueFamilyId> VulkanPhysicalDeviceInfo::findQueues(VQueueCaps caps) const {
	vector<VQueueFamilyId> out;
	out.reserve(queue_families.size());
	auto vk_caps = toVk(caps);
	for(int idx : intRange(queue_families)) {
		auto &queue = queue_families[idx];
		if((queue.queueFlags & vk_caps) == vk_caps)
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

int VulkanPhysicalDeviceInfo::findMemoryType(u32 type_bits, VMemoryFlags flags) const {
	auto prop_flags = toVk(flags);
	for(int id : intRange(mem_properties.memoryTypeCount))
		if(((1u << id) & type_bits) &&
		   (mem_properties.memoryTypes[id].propertyFlags & prop_flags) == prop_flags)
			return id;
	return -1;
}

u64 VulkanPhysicalDeviceInfo::deviceLocalMemorySize() const {
	for(uint i = 0; i < mem_properties.memoryHeapCount; i++) {
		auto &heap = mem_properties.memoryHeaps[i];
		if(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			return heap.size;
	}

	return 0;
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

double VulkanPhysicalDeviceInfo::defaultScore() const {
	double score = 0.0;
	if(findQueues(VQueueCap::graphics))
		score += 1000.0;
	if(findQueues(VQueueCap::compute))
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
#endif
	return out;
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

	VkPhysicalDeviceSubgroupProperties subgroup_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
	VkPhysicalDeviceSubgroupSizeControlProperties subgroup_control_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};

	VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	props2.pNext = &subgroup_props;
	subgroup_props.pNext = &subgroup_control_props;
	vkGetPhysicalDeviceProperties2(handle, &props2);
	subgroup_props.pNext = nullptr;
	subgroup_control_props.pNext = nullptr;
	if(!subgroup_control_props.minSubgroupSize)
		subgroup_control_props.minSubgroupSize = subgroup_control_props.maxSubgroupSize =
			subgroup_props.subgroupSize;
	out.properties = props2.properties;
	out.subgroup_props = subgroup_props;
	out.subgroup_control_props = subgroup_control_props;
	out.vendor_id = VVendorId::unknown;
	if(props2.properties.vendorID == 4098)
		out.vendor_id = VVendorId::amd;
	else if(props2.properties.vendorID == 4318)
		out.vendor_id = VVendorId::nvidia;
	else if(props2.properties.vendorID == 32902)
		out.vendor_id = VVendorId::intel;

	vkGetPhysicalDeviceMemoryProperties(handle, &out.mem_properties);

	for(auto format : all<VDepthStencilFormat>) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(handle, toVk(format), &props);
		if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			out.supported_depth_stencil_formats |= format;
	}

	for(uint f = 0; f < VK_FORMAT_ASTC_12x12_SRGB_BLOCK; f++) {
		auto format = VkFormat(f);
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(handle, format, &props);
		if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
			out.supported_color_formats.emplace_back(format);
		if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
			out.supported_color_attachment_formats.emplace_back(format);
	}

	return out;
}

static vector<VulkanPhysicalDeviceInfo> physicalDeviceInfos(VkInstance instance) {
	vector<VulkanPhysicalDeviceInfo> out;
	uint count = 0;
	vkEnumeratePhysicalDevices(instance, &count, nullptr);
	vector<VkPhysicalDevice> handles(count);
	vkEnumeratePhysicalDevices(instance, &count, handles.data());

	out.reserve(handles.size());
	for(auto handle : handles)
		out.emplace_back(physicalDeviceInfo(handle));
	return out;
}

bool VulkanInstance::isPresent() { return g_vk_storage.instance_ref_count > 0; }
VInstanceRef VulkanInstance::ref() {
	ASSERT(isPresent());
	return {};
}

Ex<VInstanceRef> VulkanInstance::create(VInstanceSetup setup) {
	auto ref = EX_PASS(g_vk_storage.allocInstance());
	EXPECT(ref->initialize(setup));
	return ref;
}

Ex<void> VulkanInstance::initialize(VInstanceSetup setup) {
	bool enable_validation = setup.debug_levels && setup.debug_types;

	if(setup.version < VulkanVersion(1, 2, 0))
		FWK_FATAL("FWK currently requires Vulkan version to be at least 1.2.0");
	auto extensions = setup.extensions;
	auto layers = setup.layers;

	if(enable_validation) {
		auto debug_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		auto debug_layer = "VK_LAYER_KHRONOS_validation";

		bool ext_available = isOneOf(debug_ext, availableExtensions());
		bool layer_available = isOneOf(debug_layer, availableLayers());
		if(ext_available && layer_available) {
			extensions.emplace_back(debug_ext);
			layers.emplace_back(debug_layer);
		} else {
			print("Vulkan validation NOT enabled:\n");
			if(!ext_available)
				print("% instance extension not available\n", debug_ext);
			if(!layer_available)
				print("% instance layer not available\n", debug_layer);
			enable_validation = false;
			setup.debug_levels = none;
			setup.debug_types = none;
		}
	}

	insertBack(extensions, vulkanSurfaceExtensions());
	makeSortedUnique(extensions);
	makeSortedUnique(layers);

	VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = "";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "fwk";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion =
		VK_MAKE_API_VERSION(0, setup.version.major, setup.version.minor, setup.version.patch);
	m_version = setup.version;

	VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	create_info.pApplicationInfo = &app_info;

	auto ext_names = transform(extensions, [](const string &str) { return str.c_str(); });
	auto layer_names = transform(layers, [](const string &str) { return str.c_str(); });

	create_info.enabledExtensionCount = ext_names.size();
	create_info.ppEnabledExtensionNames = ext_names.data();
	create_info.enabledLayerCount = layer_names.size();
	create_info.ppEnabledLayerNames = layer_names.data();

	FWK_VK_EXPECT_CALL(vkCreateInstance, &create_info, nullptr, &m_handle);

	if(enable_validation) {
		VkDebugUtilsMessengerCreateInfoEXT create_info{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
		auto dtypes = setup.debug_types;
		auto dlevels = setup.debug_levels;
		using DType = VDebugType;
		using DLevel = VDebugLevel;

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

		auto hook_func_name = "vkCreateDebugUtilsMessengerEXT";
		auto hook_messenger_func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_handle, hook_func_name);
		if(!hook_messenger_func)
			return ERROR("Cannot acquire address of function: '%'", hook_func_name);
		FWK_VK_EXPECT_CALL(hook_messenger_func, m_handle, &create_info, nullptr, &m_messenger);
	}

	m_phys_devices = physicalDeviceInfos(m_handle);
	return {};
}

VulkanInstance::VulkanInstance() = default;
VulkanInstance::~VulkanInstance() {
	if(m_messenger) {
		if(auto destroy_func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
			   m_handle, "vkDestroyDebugUtilsMessengerEXT"))
			destroy_func(m_handle, m_messenger, nullptr);
		m_messenger = nullptr;
	}
	if(m_handle) {
		vkDestroyInstance(m_handle, nullptr);
		m_handle = nullptr;
	}
}

bool VulkanInstance::valid(VPhysicalDeviceId id) const { return m_phys_devices.inRange(id); }

const VulkanPhysicalDeviceInfo &VulkanInstance::info(VPhysicalDeviceId id) const {
	return m_phys_devices[id];
}

SimpleIndexRange<VPhysicalDeviceId> VulkanInstance::physicalDeviceIds() const {
	return indexRange<VPhysicalDeviceId>(m_phys_devices.size());
}

Maybe<VPhysicalDeviceId> VulkanInstance::preferredDevice(VkSurfaceKHR target_surface,
														 vector<VQueueSetup> *out_queues) const {
	Maybe<VPhysicalDeviceId> best;
	double best_score = -1.0;

	for(auto id : physicalDeviceIds()) {
		auto &info = m_phys_devices[id];
		double score = info.defaultScore();
		if(score <= best_score)
			continue;

		auto gfx_queues = info.findQueues(VQueueCap::compute | VQueueCap::graphics);
		if(!gfx_queues)
			gfx_queues = info.findQueues(VQueueCap::graphics);
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
			vector<VQueueSetup> queue_setup;
			for(auto queue : sel_queues)
				queue_setup.emplace_back(queue, 1);
			*out_queues = std::move(queue_setup);
		}
	}

	return best;
}

Ex<VDeviceRef> VulkanInstance::createDevice(VPhysicalDeviceId phys_id, const VDeviceSetup &setup) {
	auto ref = EX_PASS(g_vk_storage.allocDevice(VInstanceRef(), phys_id));
	EXPECT(ref->initialize(setup));
	return ref;
}
}
