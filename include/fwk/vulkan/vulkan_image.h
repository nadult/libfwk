// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VImageSetup {
	VImageSetup(VkFormat format, int2 extent, int num_samples = 1,
				VImageUsageFlags usage = VImageUsage::transfer_dst | VImageUsage::sampled,
				VImageLayout layout = VImageLayout::undefined)
		: extent(extent), format(format), num_samples(num_samples), usage(usage), layout(layout) {}

	int2 extent;
	VkFormat format;
	int num_samples;
	VImageUsageFlags usage;
	VImageLayout layout;
};

class VulkanImage : public VulkanObjectBase<VulkanImage> {
  public:
	static Ex<PVImage> create(VDeviceRef, const VImageSetup &, VMemoryUsage = VMemoryUsage::device);
	static Ex<PVImage> createExternal(VDeviceRef, VkImage, const VImageSetup &);

	auto memoryBlock() { return m_memory_block; }
	int2 extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	auto usage() const { return m_usage; }
	int numSamples() const { return m_num_samples; }

  private:
	friend class VulkanDevice;
	friend class VulkanRenderGraph;
	VulkanImage(VkImage, VObjectId, VMemoryBlock, const VImageSetup &);
	~VulkanImage();

	VMemoryBlock m_memory_block;
	VkFormat m_format;
	int2 m_extent;
	int m_num_samples;
	VImageUsageFlags m_usage;
	VImageLayout m_last_layout;
	bool m_is_external = false;
};

class VulkanImageView : public VulkanObjectBase<VulkanImageView> {
  public:
	static Ex<PVImageView> create(VDeviceRef, PVImage);

	int2 extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	PVImage image() const { return m_image; }

  private:
	friend class VulkanDevice;
	VulkanImageView(VkImageView, VObjectId);
	~VulkanImageView();

	PVImage m_image;
	int2 m_extent;
	VkFormat m_format;
};
}