// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sparse_vector.h"
#include "fwk/str.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

class Image;
class VulkanRenderGraph;
using PVRenderGraph = Dynamic<VulkanRenderGraph>;

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

struct CmdDispatchCompute {
	int3 size = {1, 1, 1};
};

struct VDescriptorSet;

struct CmdBindDescriptorSet {
	uint index = 0;
	VkPipelineLayout pipe_layout = nullptr;
	VkDescriptorSet set = nullptr;
	VBindPoint bind_point = VBindPoint::graphics;
};

struct CmdSetViewport {
	IRect viewport;
	float min_depth = 0.0f;
	float max_depth = 1.0f;
};

struct CmdSetScissor {
	Maybe<IRect> scissor;
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

using Command = Variant<CmdCopy, CmdCopyImage, CmdSetViewport, CmdSetScissor, CmdBindDescriptorSet,
						CmdBindVertexBuffers, CmdBindIndexBuffer, CmdBindPipeline, CmdDraw,
						CmdDrawIndexed, CmdBeginRenderPass, CmdEndRenderPass, CmdDispatchCompute>;

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

struct VSpan {
	VSpan(PVBuffer buffer);
	VSpan(PVBuffer buffer, u32 offset);
	VSpan(PVBuffer buffer, u32 offset, u32 size) : buffer(buffer), offset(offset), size(size) {}

	PVBuffer buffer;
	u32 offset, size;
};

// TODO: new name: command queue ?
// TODO: only upload commands should be enqueue-able; All the other have to happen
// between begin & end frame
class VulkanRenderGraph {
  public:
	~VulkanRenderGraph();

	// Commands are first enqueued and only with large enough context
	// they are being performed
	void enqueue(Command);

	void bind(PVPipelineLayout, VBindPoint = VBindPoint::graphics);
	// Binds given descriptor set
	void bindDS(int index, const VDescriptorSet &);
	// Acquires new descriptor set and immediately binds it
	VDescriptorSet bindDS(int index);

	// Upload commands are handled immediately, if staging buffer is used, then copy commands
	// will be enqueued until beginFrame
	Ex<VSpan> upload(VSpan dst, CSpan<char> src);
	template <c_span TSpan, class T = SpanBase<TSpan>>
	Ex<VSpan> upload(VSpan dst, const TSpan &src) {
		return upload(dst, cspan(src).template reinterpret<char>());
	}
	Ex<void> upload(PVImage dst, const Image &src);

	Ex<VDownloadId> download(VSpan src);

	template <class T> auto operator<<(T &&cmd) { return enqueue(std::forward<T>(cmd)); }

	// This can only be called between beginFrame() & finishFrame()
	void flushCommands();

	enum class Status { init, frame_running, frame_finished };
	Status status() const { return m_status; }
	u64 frameIndex() const { return m_frame_index; }
	int swapFrameIndex() const { return m_swap_index; }

	bool isFinished(VDownloadId);
	// Returns empty vector if not ready
	PodVector<char> retrieve(VDownloadId);

  private:
	friend class VulkanDevice;
	friend class Gui;

	VulkanRenderGraph(VDeviceRef);
	VulkanRenderGraph(const VulkanRenderGraph &) = delete;
	void operator=(const VulkanRenderGraph &) = delete;

	Ex<void> initialize(VDeviceRef);

	void beginFrame();
	void finishFrame(VkSemaphore *wait_sem, VkSemaphore *out_signal_sem);

	struct FrameContext {
		VkCommandBuffer cmd_buffer;
		VCommandId cmd_id;
	};

	void perform(FrameContext &, const CmdSetViewport &);
	void perform(FrameContext &, const CmdSetScissor &);
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
	void perform(FrameContext &, const CmdDispatchCompute &);

	struct FrameSync {
		VkCommandBuffer command_buffer = nullptr;
		VkSemaphore render_finished_sem = nullptr;
		VkFence in_flight_fence = nullptr;
	};

	struct Download {
		PVBuffer buffer;
		u64 frame_index;
		bool is_ready = false;
	};

	SparseVector<Download> m_downloads;
	vector<StagingBuffer> m_staging_buffers;
	vector<Command> m_commands;
	PVPipelineLayout m_last_pipeline_layout;
	VBindPoint m_last_bind_point = VBindPoint::graphics;
	IRect m_last_viewport;

	VulkanDevice &m_device;
	VQueue m_queue;
	VkDevice m_device_handle = nullptr;
	FrameSync m_frames[VulkanLimits::num_swap_frames];
	VkCommandPool m_command_pool = nullptr;
	uint m_swap_index = 0;
	u64 m_frame_index = 0;
	Status m_status = Status::init;
};
}