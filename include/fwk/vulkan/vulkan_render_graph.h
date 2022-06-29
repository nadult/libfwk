// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class VulkanRenderGraph;
using PVRenderGraph = Dynamic<VulkanRenderGraph>;

struct CmdUploadData {
	CSpan<char> data;
	Maybe<VCommandId> copy_op;
	PVBuffer dst;
};

struct CmdUploadFunc {};

struct CmdCopy {
	PVBuffer src;
	PVBuffer dst;
	vector<VkBufferCopy> regions;
};

struct CmdBindPipeline {
	PVPipeline pipeline;
};

struct CmdBindIndexBuffer {
	PVBuffer buffer;
	// TODO
};

struct CmdBindVertexBuffers {
	uint first_binding = 0;
	vector<PVBuffer> buffers;
	vector<u64> offsets;
};

struct CmdDraw {
	int first_vertex = 0, first_instance = 0;
	int num_vertices = 0, num_instances = 1;
};

struct CmdBeginRenderPass {
	PVRenderPass render_pass;
	Maybe<IRect> render_area;
	vector<VkClearValue> clear_values;
};

struct CmdEndRenderPass {};

using Command = Variant<CmdCopy, CmdBindVertexBuffers, CmdBindPipeline, CmdDraw, CmdBeginRenderPass,
						CmdEndRenderPass>;

class VulkanRenderGraph {
  public:
	static constexpr int num_swap_frames = 2;

	~VulkanRenderGraph();

	PVSwapChain swapChain() const { return m_swap_chain; }
	PVRenderPass defaultRenderPass() const { return m_render_pass; }

	Ex<void> beginFrame();
	Ex<void> finishFrame();

	// Commands are first enqueued and only with large enough context
	// they are being performed
	void enqueue(Command);
	void operator<<(Command cmd) { return enqueue(move(cmd)); }

	// This can only be called between beginFrame() & finishFrame()
	void flushCommands();

  private:
	friend class VulkanDevice;
	VulkanRenderGraph(VDeviceRef);
	Ex<void> initialize(VDeviceRef, PVSwapChain);
	Ex<PVRenderPass> createRenderPass(VDeviceRef, PVSwapChain);

	struct FrameContext {
		PVCommandBuffer cmd_buffer;
		VCommandId cmd_id;
	};

	void perform(FrameContext &, const CmdBindIndexBuffer &);
	void perform(FrameContext &, const CmdBindVertexBuffers &);
	void perform(FrameContext &, const CmdBindPipeline &);
	void perform(FrameContext &, const CmdDraw &);
	void perform(FrameContext &, const CmdCopy &);
	void perform(FrameContext &, const CmdBeginRenderPass &);
	void perform(FrameContext &, const CmdEndRenderPass &);

	struct FrameSync {
		PVCommandBuffer command_buffer;
		PVSemaphore image_available_sem;
		PVSemaphore render_finished_sem;
		PVFence in_flight_fence;
	};

	vector<Command> m_commands;

	VDeviceRef m_device;
	PVSwapChain m_swap_chain;
	vector<PVFramebuffer> m_framebuffers;
	FrameSync m_frames[num_swap_frames];
	PVRenderPass m_render_pass;
	PVCommandPool m_command_pool;
	uint m_frame_index = 0, m_image_index = 0;
	bool m_frame_in_progress = false;
};

}