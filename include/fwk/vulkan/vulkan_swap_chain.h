// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

// TODO: naming
struct VSurfaceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
};

struct VSwapChainSetup {
	vector<VkFormat> preferred_formats = {VK_FORMAT_B8G8R8A8_SRGB};
	vector<VkFormat> preferred_depth_formats = {};
	VPresentMode preferred_present_mode = VPresentMode::fifo;
};

class VulkanSwapChain : public VulkanObjectBase<VulkanSwapChain> {
  public:
	static VSurfaceInfo surfaceInfo(VDeviceRef, VWindowRef);
	static Ex<PVSwapChain> create(VDeviceRef, VWindowRef, const VSwapChainSetup &);

	VkFormat format() const { return m_format; }
	int2 extent() const { return m_extent; }

	// Swap chain may become invalid after recreate() failed
	// In such case it shouldn't be used until it's properly recreated
	bool isValid() const;
	Ex<void> recreate();

	enum Status { initialized, invalid, image_acquired, image_presented };
	Status status() const { return m_status; }

	struct ImageInfo {
		PVImageView view;
		PVFramebuffer framebuffer;
		VkSemaphore available_sem = nullptr;
	};

	bool acquireImage();

	const ImageInfo &acquiredImage() const;
	CSpan<ImageInfo> images() const { return m_images; }
	PVFramebuffer acquiredImageFramebuffer(bool with_depth) const;

	bool presentImage(VkSemaphore wait_for_sem);

  private:
	friend class VulkanDevice;
	VulkanSwapChain(VkSwapchainKHR, VObjectId, VWindowRef, VkQueue);
	~VulkanSwapChain();

	Ex<void> initialize();
	void release();

	VWindowRef m_window;
	VSwapChainSetup m_setup;
	vector<ImageInfo> m_images;
	VkQueue m_present_queue;
	VkFormat m_format;
	int2 m_extent;
	uint m_image_index = 0;
	Status m_status = Status::initialized;
};
}