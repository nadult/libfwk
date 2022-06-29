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
	static constexpr int num_swap_frames = 2;

	~VulkanRenderGraph();

	PVSwapChain swapChain() const { return m_swap_chain; }
	PVRenderPass defaultRenderPass() const { return m_render_pass; }

	Pair<PVCommandBuffer, PVFramebuffer> beginFrame();
	void finishFrame();

  private:
	friend class VulkanDevice;
	VulkanRenderGraph(VDeviceRef);
	Ex<void> initialize(VDeviceRef, PVSwapChain);
	Ex<PVRenderPass> createRenderPass(VDeviceRef, PVSwapChain);

	struct FrameSync {
		PVCommandBuffer command_buffer;
		PVSemaphore image_available_sem;
		PVSemaphore render_finished_sem;
		PVFence in_flight_fence;
	};

	VDeviceRef m_device;
	PVSwapChain m_swap_chain;
	vector<PVFramebuffer> m_framebuffers;
	FrameSync m_frames[num_swap_frames];
	PVRenderPass m_render_pass;
	PVCommandPool m_command_pool;
	uint m_frame_index = 0, m_image_index = 0;
};

}