// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VSurfaceInfo {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> present_modes;
	bool is_minimized = false;
};

struct VSwapChainSetup {
	vector<VkFormat> preferred_formats = {VK_FORMAT_B8G8R8A8_SRGB};
	vector<VkFormat> preferred_depth_formats = {};
	VPresentMode preferred_present_mode = VPresentMode::fifo;
};

DEFINE_ENUM(VSwapChainStatus, initialized, image_acquired, ready, invalid, window_minimized);

class VulkanSwapChain : public VulkanObjectBase<VulkanSwapChain> {
  public:
	static VSurfaceInfo surfaceInfo(VDeviceRef, VWindowRef);
	static Ex<PVSwapChain> create(VDeviceRef, VWindowRef, const VSwapChainSetup &);

	VkFormat format() const { return m_format; }
	int2 extent() const { return m_extent; }

	using Status = VSwapChainStatus;
	Status status() const { return m_status; }

	struct ImageInfo {
		PVImageView view;
		PVFramebuffer framebuffer;
		VkSemaphore available_sem = nullptr;
	};
	CSpan<ImageInfo> images() const { return m_images; }

	// Returns true if image was properly acquired; false means that acquisition failed
	// and user should try again at a later time (for now the only reason is minimized window).
	// Error may also be returned when swap chain recreation failed.
	Ex<bool> acquireImage();

	// Returns true if image was properly presented; Otherwise returns false which means
	// that user should try again at a later time (starting with acquire). Error may also
	// be returned when swap chain recreation failed.
	Ex<bool> presentImage(CSpan<VkSemaphore> wait_sems);

	// This function should only be called between acquireImage() & presentImage()
	int acquiredImageIndex() const;
	VkSemaphore imageAvailableSemaphore() const {
		DASSERT_EQ(m_status, Status::image_acquired);
		return m_images[m_image_index].available_sem;
	}

	const ImageInfo &acquiredImage() const;
	PVFramebuffer acquiredImageFramebuffer(bool with_depth) const;

  private:
	friend class VulkanDevice;
	VulkanSwapChain(VkSwapchainKHR, VObjectId, VWindowRef, VkQueue);
	~VulkanSwapChain();

	Ex<void> initialize(const VSurfaceInfo &);
	Ex<void> recreate();
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