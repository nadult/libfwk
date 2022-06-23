// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VulkanQueueSetup {
	VQueueFamilyId family_id;
	int count;
};

struct VulkanDeviceSetup {
	vector<string> extensions;
	vector<VulkanQueueSetup> queues;
	Dynamic<VkPhysicalDeviceFeatures> features;
};

class VulkanDevice {
  public:
	Ex<PVSemaphore> createSemaphore(const VkSemaphoreCreateInfo &);
	Ex<PVFence> createFence(const VkFenceCreateInfo &);
	Ex<PVShaderModule> createShaderModule(CSpan<char> bytecode);

	VDeviceId id() const { return m_id; }
	VPhysicalDeviceId physId() const { return m_phys_id; }
	VkDevice handle() const { return m_handle; }
	VkPhysicalDevice physHandle() const { return m_phys_handle; }

	CSpan<Pair<VkQueue, VQueueFamilyId>> queues() const { return m_queues; }
	void nextReleasePhase();

  private:
	VulkanDevice(VDeviceId, VPhysicalDeviceId, VInstanceRef);
	~VulkanDevice();

	VulkanDevice(const VulkanDevice &) = delete;
	void operator=(const VulkanDevice &) = delete;

	Ex<void> initialize(const VulkanDeviceSetup &);

	friend class VulkanInstance;
	friend struct VulkanStorage;

	// TODO: signify which queue is for what?
	vector<Pair<VkQueue, VQueueFamilyId>> m_queues;
	VkDevice m_handle = nullptr;
	VkPhysicalDevice m_phys_handle = nullptr;

	VPhysicalDeviceId m_phys_id;
	VDeviceId m_id;
	VInstanceRef m_instance_ref;
};
}