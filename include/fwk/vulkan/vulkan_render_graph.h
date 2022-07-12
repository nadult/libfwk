// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
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

struct VDescriptorSet;

struct CmdBindDescriptorSet {
	uint index = 0;
	VkPipelineLayout pipe_layout = nullptr;
	VkDescriptorSet set = nullptr;
};

struct CmdBindVertexBuffers {
	CmdBindVertexBuffers(vector<PVBuffer> buffers, vector<uint> offsets, uint first_binding = 0)
		: buffers(move(buffers)), offsets(move(offsets)), first_binding(first_binding) {}

	vector<PVBuffer> buffers;
	vector<uint> offsets;
	uint first_binding;
};

struct CmdBindIndexBuffer {
	CmdBindIndexBuffer(PVBuffer buffer, uint offset) : buffer(buffer), offset(offset) {}

	PVBuffer buffer;
	uint offset;
};

struct CmdDraw {
	int first_vertex = 0, first_instance = 0;
	int num_vertices = 0, num_instances = 1;
};

struct CmdDrawIndexed {
	int first_index = 0, first_instance = 0;
	int num_indices = 0, num_instances = 1;
	int vertex_offset = 0;
};

struct CmdBeginRenderPass {
	PVFramebuffer framebuffer;
	PVRenderPass render_pass;
	Maybe<IRect> render_area;
	vector<VkClearValue> clear_values;
};

struct CmdEndRenderPass {};

using Command =
	Variant<CmdCopy, CmdCopyImage, CmdBindDescriptorSet, CmdBindVertexBuffers, CmdBindIndexBuffer,
			CmdBindPipeline, CmdDraw, CmdDrawIndexed, CmdBeginRenderPass, CmdEndRenderPass>;

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
	~VulkanRenderGraph();

	VkFormat swapChainFormat() const { return m_swap_chain_format; }
	PVSwapChain swapChain() const { return m_swap_chain; }

	// Commands are first enqueued and only with large enough context
	// they are being performed
	void enqueue(Command);

	void bind(PVPipelineLayout);
	// Binds given descriptor set
	void bindDS(int index, const VDescriptorSet &);
	// Acquires new descriptor set and immediately binds it
	VDescriptorSet bindDS(int index);

	// Upload commands are handled immediately, copy commands are enqueued in their place
	Ex<void> enqueue(CmdUpload);
	Ex<void> enqueue(CmdUploadImage);

	template <class T> auto operator<<(T &&cmd) { return enqueue(std::forward<T>(cmd)); }

	// This can only be called between beginFrame() & finishFrame()
	void flushCommands();
	VkCommandBuffer currentCommandBuffer();

	enum class Status { init, frame_running, frame_finished };
	Status status() const { return m_status; }
	int swapFrameIndex() const { return m_frame_index; }

	int swapImageIndex() const {
		DASSERT(m_status != Status::init);
		return m_image_index;
	}
	PVFramebuffer defaultFramebuffer() const { return m_framebuffers[swapImageIndex()]; }

  private:
	friend class VulkanDevice;
	friend class Gui;

	VulkanRenderGraph(VDeviceRef);
	VulkanRenderGraph(const VulkanRenderGraph &) = delete;
	void operator=(const VulkanRenderGraph &) = delete;

	Ex<void> initialize(VDeviceRef, PVSwapChain, PVImageView);

	void beginFrame();
	void finishFrame();

	struct FrameContext {
		VkCommandBuffer cmd_buffer;
		VCommandId cmd_id;
	};

	void perform(FrameContext &, const CmdBindDescriptorSet &);
	void perform(FrameContext &, const CmdBindIndexBuffer &);
	void perform(FrameContext &, const CmdBindVertexBuffers &);
	void perform(FrameContext &, const CmdBindPipeline &);
	void perform(FrameContext &, const CmdDraw &);
	void perform(FrameContext &, const CmdDrawIndexed &);
	void perform(FrameContext &, const CmdCopy &);
	void perform(FrameContext &, const CmdCopyImage &);
	void perform(FrameContext &, const CmdBeginRenderPass &);
	void perform(FrameContext &, const CmdEndRenderPass &);

	struct FrameSync {
		VkCommandBuffer command_buffer = nullptr;
		VkSemaphore image_available_sem = nullptr;
		VkSemaphore render_finished_sem = nullptr;
		VkFence in_flight_fence = nullptr;
	};

	vector<StagingBuffer> m_staging_buffers;
	vector<Command> m_commands;
	PVPipelineLayout m_last_pipeline_layout;

	VulkanDevice &m_device;
	VkDevice m_device_handle = nullptr;
	PVSwapChain m_swap_chain;
	FrameSync m_frames[VulkanLimits::num_swap_frames];
	vector<PVFramebuffer> m_framebuffers;
	VkCommandPool m_command_pool = nullptr;
	VkFormat m_swap_chain_format = {};
	uint m_frame_index = 0, m_image_index = 0;
	Status m_status = Status::init;
};

}