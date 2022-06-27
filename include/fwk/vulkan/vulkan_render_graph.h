// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanRenderGraph;
using PVRenderGraph = Dynamic<VulkanRenderGraph>;

class VulkanRenderGraph {
  public:
	VulkanRenderGraph(VDeviceRef);
	~VulkanRenderGraph();

	static Ex<PVRenderGraph> create(VDeviceRef, PVSwapChain);
	PVSwapChain swapChain() const { return m_swap_chain; }
	PVRenderPass defaultRenderPass() const { return m_render_pass; }
	PVCommandBuffer commandBuffer() const { return m_command_buffer; }

	PVFramebuffer beginFrame();
	void finishFrame();

  private:
	template <class> class Dynamic;
	Ex<void> initialize(VDeviceRef, PVSwapChain);

	Ex<PVRenderPass> createRenderPass(VDeviceRef, PVSwapChain);

	VDeviceRef m_device;
	PVSwapChain m_swap_chain;
	vector<PVFramebuffer> m_framebuffers;
	PVCommandPool m_command_pool;
	PVCommandBuffer m_command_buffer;
	PVSemaphore m_image_available_sem;
	PVSemaphore m_render_finished_sem;
	PVRenderPass m_render_pass;
	PVFence m_in_flight_fence;
	uint m_image_index;
};

}