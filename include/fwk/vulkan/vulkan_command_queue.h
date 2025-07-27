// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/sparse_vector.h"
#include "fwk/str.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_buffer_span.h"
#include "fwk/vulkan/vulkan_storage.h"

namespace fwk {

DEFINE_ENUM(VCommandQueueStatus, initialized, frame_running, frame_finished);

struct VClearDepthStencil {
	VClearDepthStencil(float depth, uint stencil = 0) : depth(depth), stencil(stencil) {}

	float depth;
	uint stencil;
};

struct VClearValue {
	VClearValue(FColor color) : f{color.r, color.g, color.b, color.a} {}
	VClearValue(IColor color) : VClearValue(FColor(color)) {}
	VClearValue(float4 val) : f{val.x, val.y, val.z, val.w} {}
	VClearValue(int4 val) : i{val.x, val.y, val.z, val.w} {}
	VClearValue(float val) : f{val, 0, 0, 0} {}
	VClearValue(u32 val) : u{val, 0, 0, 0} {}
	VClearValue(i32 val) : i{val, 0, 0, 0} {}
	VClearValue(VClearDepthStencil ds) {
		f[0] = ds.depth;
		u[1] = ds.stencil;
		u[2] = u[3] = 0;
	}

	union {
		float f[4];
		i32 i[4];
		u32 u[4];
	};
};

// Allows for enqueuing commands into a single queue. Commands are fed into buffers and are
// submitted either with a submit() or when the frame is finished. Each command buffer when
// submitted will wait until previously submitted buffer is finished.
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
	template <class T> PodVector<T> retrieve(VDownloadId id) {
		return retrieve(id).reinterpret<T>();
	}

	CSpan<u64> getQueryResults(u64 frame_index) const;

	// Submits all recorded commands to the queue
	void submit();

	// Waits until all submitted commands are finished
	void finish();

	// -------------------------------------------------------------------------------------------
	// ----------  Commands (must be called between beginFrame & endFrame)   ---------------------

	VkCommandBuffer bufferHandle() { return m_cur_cmd_buffer; }

	// Copy commands wil be enqueued and performed later if called

	void copy(VBufferSpan<> dst, VBufferSpan<> src);
	void copy(PVImage dst, VBufferSpan<> src, Maybe<IBox> dst_box = none, int dst_mip_level = 0,
			  VImageLayout dst_layout = VImageLayout::shader_ro);
	void addStagingBufferInTransfer(VBufferSpan<>);

	void fill(VBufferSpan<> dst, u32 value);
	template <class T> void fill(VBufferSpan<T> dst, u32 value) {
		fill(dst.template reinterpret<char>(), value);
	}

	void bind(PVPipeline);
	void bindVertices(uint first_binding, CSpan<VBufferSpan<>>);
	template <class... Args> void bindVertices(uint first_binding, VBufferSpan<Args>... spans) {
		bindVertices(first_binding, {spans.template reinterpret<char>()...});
	}

	void bindIndices(VBufferSpan<u16>);
	void bindIndices(VBufferSpan<u32>);

	void bind(PVPipelineLayout, VBindPoint = VBindPoint::graphics);
	// Binds given descriptor set
	void bindDS(int index, const VDescriptorSet &);
	// Acquires new descriptor set and immediately binds it
	VDescriptorSet bindDS(int index);

	void setPushConstants(u32 offset, VShaderStages, CSpan<char> data);
	template <class T> void setPushConstants(u32 offset, VShaderStages stages, CSpan<T> data) {
		setPushConstants(offset, stages, data.template reinterpret<char>());
	}
	template <class T> void setPushConstants(u32 offset, VShaderStages stages, const T &data) {
		auto byte_ptr = reinterpret_cast<const char *>(&data);
		setPushConstants(offset, stages, CSpan<char>(byte_ptr, sizeof(T)));
	}

	void setScissor(Maybe<IRect>);
	void setViewport(IRect, float min_depth = 0.0f, float max_depth = 1.0f);

	void draw(int num_vertices, int num_instances = 1, int first_vertex = 0,
			  int first_instance = 0);
	void drawIndexed(int num_indices, int num_instances = 1, int first_index = 0,
					 int first_instance = 0, int vertex_offset = 0);

	// TODO: pass colors & depth here directly
	void beginRenderPass(PVFramebuffer, PVRenderPass, Maybe<IRect> render_area = none,
						 CSpan<VClearValue> clear_values = {});
	void beginRenderPass(CSpan<PVImageView> attachments, PVRenderPass,
						 Maybe<IRect> render_area = none, CSpan<VClearValue> clear_values = {});

	void endRenderPass();

	void dispatchCompute(int3 size);
	void dispatchComputeIndirect(VBufferSpan<char>, u32 offset);

	void barrier(VPipeStages src, VPipeStages dst, VAccessFlags mem_src = none,
				 VAccessFlags mem_dst = none);
	void fullBarrier();

	void clearColor(PVImage, VClearValue);

	uint timestampQuery();
	void perfTimestampQuery(uint sample_id);

	Ex<VDownloadId> download(VBufferSpan<char> src);
	template <class T> Ex<VDownloadId> download(VBufferSpan<T> src) {
		return download(src.template reinterpret<char>());
	}

	Ex<PodVector<char>> download(VBufferSpan<char> src, Str unique_label, uint skip_frames = 0);
	template <class T>
	Ex<vector<T>> download(VBufferSpan<T> src, Str unique_label, uint skip_frames = 0) {
		auto result =
			EX_PASS(download(src.template reinterpret<char>(), unique_label, skip_frames));
		vector<T> out;
		if(result)
			result.template reinterpret<T>().unsafeSwap(out);
		return out;
	}

  private:
	static constexpr uint query_pool_size = 256;
	static constexpr uint query_pool_shift = 8;
	static constexpr uint num_swap_frames = VulkanLimits::num_swap_frames;

	friend class VulkanDevice;
	friend class Gui;

	VulkanCommandQueue(VulkanDevice &);
	VulkanCommandQueue(const VulkanCommandQueue &) = delete;
	void operator=(const VulkanCommandQueue &) = delete;

	Ex<void> initialize(VDeviceRef);

	struct CommandBufferInfo {
		VkCommandBuffer buffer;
		VkSemaphore semaphore;
		VkFence fence;
	};

	struct SwapFrame {
		// Commands from previous frame with the same swap index
		vector<CommandBufferInfo> previous_commands;
		vector<CommandBufferInfo> commands;
		vector<VkQueryPool> query_pools;
		vector<Pair<uint, uint>> perf_queries;
		vector<u64> query_results;
		i64 perf_frame_id = 0;
		uint query_count = 0;
		int num_waited_fences = 0;
	};

	CommandBufferInfo acquireCommands();
	void recycleCommands(CommandBufferInfo);
	void beginCommands();
	void submitCommands(VkSemaphore, VPipeStages);

	void waitForSwapFrameAvailable();

	void beginFrame();
	VkSemaphore finishFrame(VkSemaphore image_available_semaphore);

	void updateAttachmentsLayouts(bool final);

	struct Download {
		PVBuffer buffer;
		u64 frame_index;
		bool is_ready = false;
	};

	struct LabeledDownload {
		string label;
		vector<VDownloadId> ids;
		uint last_frame;
	};

	struct CmdBindDescriptorSet {
		uint index = 0;
		VkPipelineLayout pipe_layout = nullptr;
		VkDescriptorSet set = nullptr;
		VBindPoint bind_point = VBindPoint::graphics;
	};

	PVRenderPass m_current_render_pass;
	PVFramebuffer m_current_framebuffer;

	vector<CommandBufferInfo> m_recycled_commands;
	SparseVector<Download> m_downloads;
	vector<LabeledDownload> m_labeled_downloads;
	vector<PVBuffer> m_staging_buffers;
	PVPipelineLayout m_last_pipeline_layout;
	VBindPoint m_last_bind_point = VBindPoint::graphics;
	IRect m_last_viewport;

	VulkanDevice &m_device;
	VQueue m_queue;
	VkDevice m_device_handle = nullptr;
	// TODO: swap_frames
	SwapFrame m_frames[num_swap_frames];
	VkSemaphore m_last_submitted_semaphore = nullptr;
	VkCommandPool m_command_pool = nullptr;
	VkCommandBuffer m_cur_cmd_buffer = nullptr;
	uint m_swap_index = 0, m_frame_index = 0;
	double m_timestamp_period = 1.0;
	Status m_status = Status::initialized;
};
}
