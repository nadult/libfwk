// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VImageSetup {
	VImageSetup(VkFormat format, int2 extent, uint num_samples = 1,
				VImageUsageFlags usage = VImageUsage::transfer_dst | VImageUsage::sampled,
				VImageLayout layout = VImageLayout::undefined)
		: extent(extent), format(format), num_samples(num_samples), usage(usage), layout(layout) {}

	VImageSetup(VDepthStencilFormat ds_format, int2 extent, uint num_samples = 1);

	int2 extent;
	VkFormat format;
	uint num_samples;
	VImageUsageFlags usage;
	VImageLayout layout;
};

class VulkanImage : public VulkanObjectBase<VulkanImage> {
  public:
	static Ex<PVImage> create(VDeviceRef, const VImageSetup &, VMemoryUsage = VMemoryUsage::device);
	static PVImage createExternal(VDeviceRef, VkImage, const VImageSetup &);

	auto memoryBlock() { return m_memory_block; }
	int2 extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	auto usage() const { return m_usage; }
	uint numSamples() const { return m_num_samples; }

	// External image may become invalid (for example when swap chain is destroyed)
	bool isValid() const { return m_is_valid; }
	Maybe<VDepthStencilFormat> depthStencilFormat() const;

  private:
	friend class VulkanSwapChain;
	friend class VulkanDevice;
	friend class VulkanCommandQueue;
	VulkanImage(VkImage, VObjectId, VMemoryBlock, const VImageSetup &);
	~VulkanImage();

	VMemoryBlock m_memory_block;
	VkFormat m_format;
	int2 m_extent;
	uint m_num_samples;
	VImageUsageFlags m_usage;
	VImageLayout m_last_layout;
	bool m_is_external = false;
	bool m_is_valid = true;
};

class VulkanImageView : public VulkanObjectBase<VulkanImageView> {
  public:
	static PVImageView create(VDeviceRef, PVImage);

	int2 extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	PVImage image() const { return m_image; }
	uint numSamples() const { return m_num_samples; }

  private:
	friend class VulkanDevice;
	VulkanImageView(VkImageView, VObjectId);
	~VulkanImageView();

	PVImage m_image;
	int2 m_extent;
	uint m_num_samples;
	VkFormat m_format;
};
}