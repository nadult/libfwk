// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct ImageSetup {
	VkFormat format;
	VImageUsageFlags usage = VImageUsage::transfer_dst | VImageUsage::sampled;
	VImageLayout layout = VImageLayout::undefined;
	int num_samples = 1;
};

class VulkanImage : public VulkanObjectBase<VulkanImage> {
  public:
	static Ex<PVImage> create(VDeviceRef, VkExtent2D, const ImageSetup &);
	static Ex<PVImage> createExternal(VDeviceRef, VkImage, VkExtent2D, const ImageSetup &);

	VkMemoryRequirements memoryRequirements() const;
	Ex<void> bindMemory(PVDeviceMemory);

	auto memoryFlags() { return m_memory_flags; }
	VkExtent2D extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	auto usage() const { return m_usage; }
	int numSamples() const { return m_num_samples; }

  private:
	friend class VulkanDevice;
	friend class VulkanRenderGraph;
	VulkanImage(VkImage, VObjectId, VkExtent2D, const ImageSetup &);
	~VulkanImage();

	VkFormat m_format;
	VkExtent2D m_extent;
	int m_num_samples;
	VImageUsageFlags m_usage;
	PVDeviceMemory m_memory;
	VMemoryFlags m_memory_flags;
	VImageLayout m_last_layout;
	bool m_is_external = false;
};

class VulkanImageView : public VulkanObjectBase<VulkanImageView> {
  public:
	static Ex<PVImageView> create(VDeviceRef, PVImage);

	VkExtent2D extent() const { return m_extent; }
	VkFormat format() const { return m_format; }
	PVImage image() const { return m_image; }

  private:
	friend class VulkanDevice;
	VulkanImageView(VkImageView, VObjectId, PVImage, VkFormat, VkExtent2D);
	~VulkanImageView();

	PVImage m_image;
	VkExtent2D m_extent;
	VkFormat m_format;
};
}