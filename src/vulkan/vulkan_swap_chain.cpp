// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_swap_chain.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

VulkanSwapChain::VulkanSwapChain(VkSwapchainKHR handle, VObjectId id, VWindowRef window)
	: VulkanObjectBase(handle, id), m_window(window) {}

VulkanSwapChain::~VulkanSwapChain() {
	deferredHandleRelease<VkSwapchainKHR, vkDestroySwapchainKHR>();
}

VulkanSurfaceInfo VulkanSwapChain::surfaceInfo(VDeviceRef device, VWindowRef window) {
	auto surf_handle = window->surfaceHandle();
	auto phys_handle = device->physHandle();

	VulkanSurfaceInfo out;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_handle, surf_handle, &out.capabilities);
	uint count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, nullptr);
	out.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_handle, surf_handle, &count, out.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count, nullptr);
	out.present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_handle, surf_handle, &count,
											  out.present_modes.data());
	return out;
}

PVSwapChain VulkanSwapChain::create(VDeviceRef device, VWindowRef window,
									const VulkanSwapChainSetup &setup) {
	auto out = device->createObject(VkSwapchainKHR(nullptr), window);
	out->m_setup = setup;
	out->initialize();
	return out;
}

void VulkanSwapChain::initialize() {
	auto device = deviceRef();
	auto surf_info = surfaceInfo(device, m_window);

	VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	VImageUsageFlags usage = VImageUsage::color_att;
	auto layout = VImageLayout::color_att;

	auto &phys_info = VulkanInstance::ref()->info(device->physId());

	ci.surface = m_window->surfaceHandle();
	ci.minImageCount = surf_info.capabilities.minImageCount;
	if(ci.minImageCount == 1)
		ci.minImageCount = min(ci.minImageCount + 1, surf_info.capabilities.maxImageCount);
	ci.imageFormat = surf_info.formats[0].format;
	ci.imageColorSpace = surf_info.formats[0].colorSpace;
	ci.imageExtent = surf_info.capabilities.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = toVk(usage);
	ci.preTransform = surf_info.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.clipped = VK_TRUE;

	bool valid_preferred = isOneOf(toVk(m_setup.preferred_present_mode), surf_info.present_modes);
	ci.presentMode = toVk(valid_preferred ? m_setup.preferred_present_mode : VPresentMode::fifo);

	bool found_preferred_format = false;
	for(int i = 0; i < m_setup.preferred_formats.size() && !found_preferred_format; i++) {
		auto preferred = m_setup.preferred_formats[i];
		for(auto format : surf_info.formats)
			if(format.format == preferred) {
				ci.imageFormat = format.format;
				ci.imageColorSpace = format.colorSpace;
				break;
			}
	}

	FWK_VK_CALL(vkCreateSwapchainKHR, device, &ci, nullptr, &m_handle);

	vector<VkImage> images;
	uint num_images = 0;
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, nullptr);
	images.resize(num_images);
	vkGetSwapchainImagesKHR(device, m_handle, &num_images, images.data());

	m_image_views.reserve(num_images);
	for(auto image_handle : images) {
		VImageSetup setup(ci.imageFormat, fromVk(ci.imageExtent), 1, usage, layout);
		auto image = VulkanImage::createExternal(device, image_handle, setup);
		auto view = VulkanImageView::create(device, image);
		m_image_views.emplace_back(view);
	}
}

void VulkanSwapChain::recreate() {
	vkDeviceWaitIdle(deviceHandle());
	m_image_views.clear(); // TODO: destroy immediately
	vkDestroySwapchainKHR(deviceHandle(), m_handle, nullptr);
	m_handle = nullptr;
	initialize();
}
}