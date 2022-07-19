// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sparse_vector.h"
#include "fwk/str.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

struct VSpan {
	VSpan(PVBuffer buffer);
	VSpan(PVBuffer buffer, u32 offset);
	VSpan(PVBuffer buffer, u32 offset, u32 size) : buffer(buffer), offset(offset), size(size) {}

	PVBuffer buffer;
	u32 offset, size;
};

DEFINE_ENUM(VCommandQueueStatus, initialized, frame_running, frame_finished);

class VulkanCommandQueue {
  public:
	~VulkanCommandQueue();
	using Status = VCommandQueueStatus;

	Status status() const { return m_status; }
	u64 frameIndex() const { return m_frame_index; }
	int swapFrameIndex() const { return m_swap_index; }

	bool isFinished(VDownloadId);
	// Returns empty vector if not ready
	PodVector<char> retrieve(VDownloadId);

	// -------------------------------------------------------------------------------------------
	// ----------  Commands (must be called between beginFrame & endFrame)   ---------------------

	void copy(VSpan dst, VSpan src);
	void copy(PVImage dst, VSpan src, VImageLayout dst_layout = VImageLayout::shader_ro);

	void bind(PVPipeline);
	void bindVertices(CSpan<VSpan>, uint first_binding = 0);
	void bindIndices(VSpan);

	void bind(PVPipelineLayout, VBindPoint = VBindPoint::graphics);
	// Binds given descriptor set
	void bindDS(int index, const VDescriptorSet &);
	// Acquires new descriptor set and immediately binds it
	VDescriptorSet bindDS(int index);

	void setScissor(Maybe<IRect>);
	void setViewport(IRect, float min_depth = 0.0f, float max_depth = 1.0f);

	void draw(int num_vertices, int num_instances = 1, int first_vertex = 0,
			  int first_instance = 0);
	void drawIndexed(int num_indices, int num_instances = 1, int first_index = 0,
					 int first_instance = 0, int vertex_offset = 0);

	// TODO: pass colors & depth here directly
	void beginRenderPass(PVFramebuffer, PVRenderPass, Maybe<IRect> render_area,
						 CSpan<VkClearValue> clear_values);
	void endRenderPass();

	void dispatchCompute(int3 size);

	Ex<VDownloadId> download(VSpan src);

	// -------------------------------------------------------------------------------------------
	// ----------  Upload commands (can be called anytime)   -------------------------------------

	// Upload commands are handled immediately, if staging buffer is used, then copy commands
	// will be enqueued until beginFrame
	Ex<VSpan> upload(VSpan dst, CSpan<char> src);
	template <c_span TSpan, class T = SpanBase<TSpan>>
	Ex<VSpan> upload(VSpan dst, const TSpan &src) {
		return upload(dst, cspan(src).template reinterpret<char>());
	}
	Ex<void> upload(PVImage dst, const Image &src,
					VImageLayout dst_layout = VImageLayout::shader_ro);

  private:
	friend class VulkanDevice;
	friend class Gui;

	VulkanCommandQueue(VDeviceRef);
	VulkanCommandQueue(const VulkanCommandQueue &) = delete;
	void operator=(const VulkanCommandQueue &) = delete;

	Ex<void> initialize(VDeviceRef);

	void beginFrame();
	void finishFrame(VkSemaphore *wait_sem, VkSemaphore *out_signal_sem);

	struct FrameContext {
		VkCommandBuffer cmd_buffer;
		VCommandId cmd_id;
	};

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

	struct CmdCopyBuffer {
		VSpan dst, src;
	};

	struct CmdCopyImage {
		PVImage dst;
		VSpan src;
		VImageLayout dst_layout;
	};

	using CmdCopy = Variant<CmdCopyBuffer, CmdCopyImage>;

	struct CmdBindDescriptorSet {
		uint index = 0;
		VkPipelineLayout pipe_layout = nullptr;
		VkDescriptorSet set = nullptr;
		VBindPoint bind_point = VBindPoint::graphics;
	};

	SparseVector<Download> m_downloads;
	vector<CmdCopy> m_copy_queue;
	vector<PVBuffer> m_staging_buffers;
	PVPipelineLayout m_last_pipeline_layout;
	VBindPoint m_last_bind_point = VBindPoint::graphics;
	IRect m_last_viewport;

	VulkanDevice &m_device;
	VQueue m_queue;
	VkDevice m_device_handle = nullptr;
	FrameSync m_frames[VulkanLimits::num_swap_frames];
	VkCommandPool m_command_pool = nullptr;
	VkCommandBuffer m_cur_cmd_buffer = nullptr;
	uint m_swap_index = 0, m_frame_index = 0;
	Status m_status = Status::initialized;
};
}