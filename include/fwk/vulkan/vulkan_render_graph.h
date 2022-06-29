// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys/expected.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class Image;
class VulkanRenderGraph;
using PVRenderGraph = Dynamic<VulkanRenderGraph>;

// TODO: option to pass function/functor which copies some data to dst buffer
struct CmdUpload {
	CmdUpload(CSpan<char> data, PVBuffer dst, u64 offset = 0)
		: data(data), dst(dst), offset(offset) {}
	template <c_span TSpan, class T = SpanBase<TSpan>>
	CmdUpload(const TSpan &span, PVBuffer dst, u64 offset = 0)
		: data(cspan(span).template reinterpret<char>()), dst(dst), offset(offset) {}

	CSpan<char> data;
	PVBuffer dst;
	u64 offset;
};

struct CmdUploadImage {
	CmdUploadImage(const Image &image, PVImage dst) : image(image), dst(dst) {}

	const Image &image;
	PVImage dst;
};

struct CmdCopy {
	PVBuffer src;
	PVBuffer dst;
	vector<VkBufferCopy> regions;
};

struct CmdCopyImage {
	PVBuffer src;
	PVImage dst;
	Maybe<VImageLayout> dst_layout;
};

struct CmdBindPipeline {
	PVPipeline pipeline;
};

struct CmdBindIndexBuffer {
	PVBuffer buffer;
	// TODO
};

struct CmdBindVertexBuffers {
	CmdBindVertexBuffers(vector<PVBuffer> buffers, vector<u64> offsets, uint first_binding = 0)
		: buffers(move(buffers)), offsets(move(offsets)), first_binding(first_binding) {}

	uint first_binding;
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

using Command = Variant<CmdCopy, CmdCopyImage, CmdBindVertexBuffers, CmdBindPipeline, CmdDraw,
						CmdBeginRenderPass, CmdEndRenderPass>;

class StagingBuffer {
  public:
	struct FuncUpload {
		using FillFunc = void (*)(void *dst_data);
		FillFunc filler;
		i64 size = 0;
	};
	struct DataUpload {
		CSpan<char> data;
	};
	using Upload = Variant<FuncUpload, DataUpload>;

	PVBuffer buffer;
};

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

	// Upload commands are handled immediately, copy commands are enqueued in their place
	Ex<void> enqueue(CmdUpload);
	Ex<void> enqueue(CmdUploadImage);

	template <class T> auto operator<<(T &&cmd) { return enqueue(std::forward<T>(cmd)); }

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
	void perform(FrameContext &, const CmdCopyImage &);
	void perform(FrameContext &, const CmdBeginRenderPass &);
	void perform(FrameContext &, const CmdEndRenderPass &);

	struct FrameSync {
		PVCommandBuffer command_buffer;
		PVSemaphore image_available_sem;
		PVSemaphore render_finished_sem;
		PVFence in_flight_fence;
	};

	vector<StagingBuffer> m_staging_buffers;
	vector<Command> m_commands;

	VulkanDevice &m_device;
	VkDevice m_device_handle;
	PVSwapChain m_swap_chain;
	vector<PVFramebuffer> m_framebuffers;
	FrameSync m_frames[num_swap_frames];
	PVRenderPass m_render_pass;
	PVCommandPool m_command_pool;
	uint m_frame_index = 0, m_image_index = 0;
	bool m_frame_in_progress = false;
};

}