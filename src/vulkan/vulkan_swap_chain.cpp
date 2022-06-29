// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_swap_chain.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_window.h"

#include <vulkan/vulkan.h>

namespace fwk {

VulkanSwapChain::VulkanSwapChain(VkSwapchainKHR handle, VObjectId id, VWindowRef window,
								 vector<PVImageView> image_views)
	: VulkanObjectBase(handle, id), m_window(window), m_image_views(move(image_views)) {}

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

Ex<PVSwapChain> VulkanSwapChain::create(VDeviceRef device, VWindowRef window,
										const VulkanSwapChainSetup &setup) {
	VkSwapchainKHR handle;
	VkSwapchainCreateInfoKHR ci{};
	VImageUsage usage = VImageUsageFlag::color_attachment;
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = window->surfaceHandle();
	ci.minImageCount = 2;
	ci.imageFormat = setup.surface_format.format;
	ci.imageColorSpace = setup.surface_format.colorSpace;
	ci.imageExtent = toVkExtent(window->extent());
	ci.imageArrayLayers = 1;
	ci.imageUsage = usage.bits;
	ci.preTransform = setup.transform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.clipped = VK_TRUE;
	ci.presentMode = setup.present_mode;

	// TODO: separate class for swap chain ?
	if(vkCreateSwapchainKHR(device, &ci, nullptr, &handle) != VK_SUCCESS)
		FATAL("Error when creating swap chain");

	vector<VkImage> images;
	uint num_images = 0;
	vkGetSwapchainImagesKHR(device, handle, &num_images, nullptr);
	images.resize(num_images);
	vkGetSwapchainImagesKHR(device, handle, &num_images, images.data());

	vector<PVImageView> image_views;
	image_views.reserve(num_images);
	for(auto image_handle : images) {
		ImageSetup setup{.format = ci.imageFormat, .num_samples = 1, .usage = usage};
		auto image =
			EX_PASS(VulkanImage::createExternal(device, image_handle, ci.imageExtent, setup));
		auto view = EX_PASS(VulkanImageView::create(device, image));
		image_views.emplace_back(view);
	}

	return device->createObject(handle, window, move(image_views));
}
}