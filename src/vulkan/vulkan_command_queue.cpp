// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_command_queue.h"

#include "fwk/index_range.h"
#include "fwk/perf/thread_context.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"
#include "fwk/vulkan/vulkan_memory_manager.h"
#include "fwk/vulkan/vulkan_pipeline.h"

// TODO: add function for clearing labeled downloads

namespace fwk {

VulkanCommandQueue::VulkanCommandQueue(VDeviceRef device)
	: m_device(*device), m_device_handle(device) {}

VulkanCommandQueue::~VulkanCommandQueue() {
	DASSERT(m_status != Status::frame_running);
	if(!m_device_handle)
		return;
	vkDeviceWaitIdle(m_device_handle);
	for(auto &frame : m_frames) {
		vkDestroySemaphore(m_device_handle, frame.render_finished_sem, nullptr);
		vkDestroyFence(m_device_handle, frame.in_flight_fence, nullptr);
		for(auto pool : frame.query_pools)
			vkDestroyQueryPool(m_device_handle, pool, nullptr);
	}
	vkDestroyCommandPool(m_device_handle, m_command_pool, nullptr);
}

Ex<void> VulkanCommandQueue::initialize(VDeviceRef device) {
	auto queue = device->findFirstQueue(VQueueCap::graphics);
	EXPECT(queue);
	m_queue = *queue;

	auto pool_flags = VCommandPoolFlag::reset_command | VCommandPoolFlag::transient;
	m_command_pool = createVkCommandPool(device, m_queue.family_id, pool_flags);
	for(auto &frame : m_frames) {
		frame.command_buffer = allocVkCommandBuffer(device, m_command_pool);
		frame.render_finished_sem = createVkSemaphore(device);
		frame.in_flight_fence = createVkFence(device, true);
	}
	m_timestamp_period = m_device.physInfo().properties.limits.timestampPeriod;

	return {};
}

void VulkanCommandQueue::waitForFrameFence() {
	DASSERT(m_status != Status::frame_running);
	auto &frame = m_frames[m_swap_index];
	VkFence fences[] = {frame.in_flight_fence};
	vkWaitForFences(m_device_handle, 1, fences, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device_handle, 1, fences);
}

void VulkanCommandQueue::beginFrame() {
	DASSERT(m_status != Status::frame_running);
	auto &frame = m_frames[m_swap_index];

	for(auto &download : m_downloads)
		if(download.frame_index + VulkanLimits::num_swap_frames <= m_frame_index)
			download.is_ready = true;

	vkResetCommandBuffer(frame.command_buffer, 0);
	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	FWK_VK_CALL(vkBeginCommandBuffer, frame.command_buffer, &bi);
	m_cur_cmd_buffer = frame.command_buffer;
	m_status = Status::frame_running;

	if(frame.query_count > 0) {
		PodVector<u64> new_data(frame.query_count);
		uint cur_count = 0;
		for(auto pool : frame.query_pools) {
			uint pool_count = min(frame.query_count - cur_count, query_pool_size);
			if(pool_count == 0)
				break;

			size_t data_size = sizeof(u64) * pool_count;
			auto data_offset = new_data.data() + cur_count;
			cur_count += pool_count;

			FWK_VK_CALL(vkGetQueryPoolResults, m_device_handle, pool, 0, pool_count, data_size,
						data_offset, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			vkCmdResetQueryPool(m_cur_cmd_buffer, pool, 0, pool_count);
		}

		vector<u64> new_data_vec;
		new_data.unsafeSwap(frame.query_results);
	}
	frame.query_count = 0;

	auto *perf_tc = perf::ThreadContext::current();
	if(frame.perf_queries) {
		auto first_value = frame.query_results[0];
		if(perf_tc) {
			auto sample_values =
				transform(frame.perf_queries, [&](Pair<uint, uint> sample_query_id) {
					auto value = frame.query_results[sample_query_id.second] - first_value;
					value *= m_timestamp_period;
					return pair{sample_query_id.first, u64(value)};
				});
			perf_tc->resolveGpuScopes(frame.perf_frame_id, sample_values);
		}
		frame.perf_queries.clear();
	}
	frame.perf_frame_id = perf_tc ? perf_tc->frameId() : -1;

	auto first_query = timestampQuery();
	PASSERT(first_query == 0);

	// Flushing copy commands
	for(auto &copy_command : m_copy_queue) {
		if(const CmdCopyBuffer *cmd = copy_command)
			copy(cmd->dst, cmd->src);
		else if(const CmdCopyImage *cmd = copy_command)
			copy(cmd->dst, cmd->src, cmd->dst_mip_level, cmd->dst_layout);
	}
	m_copy_queue.clear();
}

void VulkanCommandQueue::finishFrame(VkSemaphore *wait_sem, VkSemaphore *out_signal_sem) {
	DASSERT(m_status == Status::frame_running);
	m_status = Status::frame_finished;

	auto &frame = m_frames[m_swap_index];
	FWK_VK_CALL(vkEndCommandBuffer, frame.command_buffer);

	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &frame.command_buffer;
	if(wait_sem) {
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_sem;
	}
	if(out_signal_sem) {
		*out_signal_sem = frame.render_finished_sem;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = out_signal_sem;
	}

	FWK_VK_CALL(vkQueueSubmit, m_queue.handle, 1, &submit_info, frame.in_flight_fence);
	m_swap_index = (m_swap_index + 1) % VulkanLimits::num_swap_frames;
	m_frame_index++;

	// TODO: staging buffers destruction should be hooked to fence
	m_staging_buffers.clear();
	m_last_pipeline_layout.reset();
	m_last_bind_point = VBindPoint::graphics;
	m_last_viewport = {};
}

CSpan<u64> VulkanCommandQueue::getQueryResults(u64 frame_index) const {
	i64 frame_indices[num_swap_frames];
	for(int i : intRange(num_swap_frames)) {
		uint idx = (i + m_swap_index) % num_swap_frames;
		frame_indices[idx] = i64(m_frame_index) - num_swap_frames - i;
	}
	if(m_status != Status::frame_running)
		frame_indices[m_swap_index] -= num_swap_frames;

	for(int i : intRange(num_swap_frames))
		if(frame_index == frame_indices[i]) {
			uint idx = (i + m_swap_index) % num_swap_frames;
			return m_frames[idx].query_results;
		}
	return {};
}

bool VulkanCommandQueue::isFinished(VDownloadId download_id) {
	PASSERT(m_downloads.valid(download_id));
	auto &download = m_downloads[download_id];
	return download.is_ready;
}

PodVector<char> VulkanCommandQueue::retrieve(VDownloadId download_id) {
	PASSERT(m_downloads.valid(download_id));
	auto &download = m_downloads[download_id];
	if(!download.is_ready)
		return {};
	auto mem_block = download.buffer->memoryBlock();
	PodVector<char> out(download.buffer->size());
	auto src_memory = m_device.memory().readAccessMemory(mem_block);
	fwk::copy(out, src_memory.subSpan(0, out.size()));
	m_downloads.erase(download_id);
	return out;
}

// -------------------------------------------------------------------------------------------
// ----------  Commands ----------------------------------------------------------------------

VkCommandBuffer VulkanCommandQueue::bufferHandle() {
	DASSERT_EQ(m_status, Status::frame_running);
	return m_cur_cmd_buffer;
}

void VulkanCommandQueue::copy(VBufferSpan<char> dst, VBufferSpan<char> src) {
	DASSERT_LE(src.size(), dst.size());
	if(m_status != Status::frame_running) {
		m_copy_queue.emplace_back(CmdCopyBuffer{dst, src});
		return;
	}

	VkBufferCopy copy_params{src.byteOffset(), dst.byteOffset(), VkDeviceSize(src.byteSize())};
	vkCmdCopyBuffer(m_cur_cmd_buffer, src.buffer(), dst.buffer(), 1, &copy_params);
}

void VulkanCommandQueue::copy(PVImage dst, VBufferSpan<> src, int dst_mip_level,
							  VImageLayout dst_layout) {
	if(m_status != Status::frame_running) {
		m_copy_queue.emplace_back(CmdCopyImage{dst, src, dst_mip_level, dst_layout});
		return;
	}

	VkBufferImageCopy region{};
	region.bufferOffset = src.byteOffset();
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = dst_mip_level;
	auto mip_size = dst->mipSize(dst_mip_level);
	region.imageExtent = {uint(mip_size.x), uint(mip_size.y), uint(mip_size.z)};

	auto copy_layout = VImageLayout::transfer_dst;
	dst->transitionLayout(copy_layout, dst_mip_level);
	vkCmdCopyBufferToImage(m_cur_cmd_buffer, src.buffer(), dst, toVk(copy_layout), 1, &region);
	// TODO: do this transition right before we're going to do something with this texture?
	dst->transitionLayout(dst_layout, dst_mip_level);
}

void VulkanCommandQueue::fill(VBufferSpan<> dst, u32 value) {
	PASSERT(m_status == Status::frame_running);
	vkCmdFillBuffer(m_cur_cmd_buffer, dst.buffer(), dst.byteOffset(), dst.byteSize(), value);
}

void VulkanCommandQueue::bind(PVPipelineLayout layout, VBindPoint bind_point) {
	m_last_pipeline_layout = layout;
	m_last_bind_point = bind_point;
}

void VulkanCommandQueue::bindDS(int index, const VDescriptorSet &ds) {
	vkCmdBindDescriptorSets(m_cur_cmd_buffer, toVk(m_last_bind_point), m_last_pipeline_layout,
							uint(index), 1, &ds.handle, 0, nullptr);
}

VDescriptorSet VulkanCommandQueue::bindDS(int index) {
	DASSERT(m_last_pipeline_layout);
	CSpan<VDSLId> dsls = m_last_pipeline_layout->descriptorSetLayouts();
	DASSERT(index >= 0 && index < dsls.size());
	auto ds = m_device.acquireSet(dsls[index]);
	bindDS(index, ds);
	return ds;
}

Ex<VDownloadId> VulkanCommandQueue::download(VBufferSpan<char> src) {
	PASSERT(m_status == Status::frame_running);
	auto buffer = EX_PASS(
		VulkanBuffer::create(m_device, src.size(), VBufferUsage::transfer_dst, VMemoryUsage::host));
	copy(buffer, src);
	auto id = m_downloads.emplace(move(buffer), m_frame_index);
	return VDownloadId(id);
}

Ex<PodVector<char>> VulkanCommandQueue::download(VBufferSpan<char> src, Str unique_label,
												 uint skip_frames) {
	if(!src)
		return PodVector<char>();
	LabeledDownload *current = nullptr;
	for(auto &ld : m_labeled_downloads)
		if(ld.label == unique_label) {
			current = &ld;
			break;
		}
	if(!current) {
		current = &m_labeled_downloads.emplace_back();
		current->label = unique_label;
		current->last_frame = m_frame_index;
	}

	PodVector<char> out;
	while(current->ids) {
		auto id = current->ids.front();
		out = retrieve<char>(id);
		if(!out)
			break;
		current->ids.erase(current->ids.begin());
	}

	uint frame_counter = m_frame_index - current->last_frame;
	if(skip_frames && frame_counter % (skip_frames + 1) == 0) {
		current->ids.emplace_back(EX_PASS(download(src)));
		current->last_frame = m_frame_index;
	}

	return out;
}

void VulkanCommandQueue::setViewport(IRect viewport, float min_depth, float max_depth) {
	PASSERT(m_status == Status::frame_running);
	VkViewport vk_viewport{float(viewport.x()),		 float(viewport.y()), float(viewport.width()),
						   float(viewport.height()), min_depth,			  max_depth};
	m_last_viewport = viewport;
	vkCmdSetViewport(m_cur_cmd_buffer, 0, 1, &vk_viewport);
}

void VulkanCommandQueue::setScissor(Maybe<IRect> scissor) {
	PASSERT(m_status == Status::frame_running);
	DASSERT(scissor || !m_last_viewport.empty());
	IRect rect = scissor.orElse(m_last_viewport);
	VkRect2D vk_rect{{rect.x(), rect.y()}, {uint(rect.width()), uint(rect.height())}};
	vkCmdSetScissor(m_cur_cmd_buffer, 0, 1, &vk_rect);
}

void VulkanCommandQueue::bindIndices(VBufferSpan<u16> span) {
	PASSERT(m_status == Status::frame_running);
	vkCmdBindIndexBuffer(m_cur_cmd_buffer, span.buffer(), span.byteOffset(),
						 VkIndexType::VK_INDEX_TYPE_UINT16);
}

void VulkanCommandQueue::bindIndices(VBufferSpan<u32> span) {
	PASSERT(m_status == Status::frame_running);
	vkCmdBindIndexBuffer(m_cur_cmd_buffer, span.buffer(), span.byteOffset(),
						 VkIndexType::VK_INDEX_TYPE_UINT32);
}

void VulkanCommandQueue::bindVertices(uint first_binding, CSpan<VBufferSpan<char>> vbuffers) {
	PASSERT(m_status == Status::frame_running);
	static constexpr int max_bindings = 32;
	VkBuffer buffers[max_bindings];
	VkDeviceSize offsets_64[max_bindings];

	DASSERT(vbuffers.size() <= max_bindings);
	for(int i : intRange(vbuffers)) {
		auto &vbuffer = vbuffers[i];
		if(vbuffer) {
			buffers[i] = vbuffers[i].buffer();
			offsets_64[i] = vbuffers[i].byteOffset();
		} else {
			buffers[i] = m_device.dummyBuffer();
			offsets_64[i] = 0;
		}
	}
	vkCmdBindVertexBuffers(m_cur_cmd_buffer, first_binding, vbuffers.size(), buffers, offsets_64);
}

void VulkanCommandQueue::bind(PVPipeline pipeline) {
	PASSERT(m_status == Status::frame_running);
	m_last_pipeline_layout = pipeline->layout();
	m_last_bind_point = pipeline->bindPoint();
	vkCmdBindPipeline(m_cur_cmd_buffer, toVk(m_last_bind_point), pipeline);
}

void VulkanCommandQueue::draw(int num_vertices, int num_instances, int first_vertex,
							  int first_instance) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDraw(m_cur_cmd_buffer, num_vertices, num_instances, first_vertex, first_instance);
}

void VulkanCommandQueue::drawIndexed(int num_indices, int num_instances, int first_index,
									 int first_instance, int vertex_offset) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDrawIndexed(m_cur_cmd_buffer, num_indices, num_instances, first_index, vertex_offset,
					 first_instance);
}

// TODO: add cache for renderpasses, add renderpass for framebuffer, interface for clear values
void VulkanCommandQueue::beginRenderPass(PVFramebuffer framebuffer, PVRenderPass render_pass,
										 Maybe<IRect> render_area,
										 CSpan<VClearValue> clear_values) {
	static_assert(sizeof(VClearValue) == sizeof(VkClearValue));

	PASSERT(m_status == Status::frame_running);
	VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	bi.renderPass = render_pass;
	if(render_area)
		bi.renderArea = toVkRect(*render_area);
	else
		bi.renderArea.extent = toVkExtent(framebuffer->size());
	bi.clearValueCount = clear_values.size();
	bi.pClearValues = reinterpret_cast<const VkClearValue *>(clear_values.data());
	bi.framebuffer = framebuffer;
	vkCmdBeginRenderPass(m_cur_cmd_buffer, &bi, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandQueue::endRenderPass() {
	PASSERT(m_status == Status::frame_running);
	vkCmdEndRenderPass(m_cur_cmd_buffer);
}

void VulkanCommandQueue::dispatchCompute(int3 size) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDispatch(m_cur_cmd_buffer, size.x, size.y, size.z);
}

void VulkanCommandQueue::dispatchComputeIndirect(VBufferSpan<char> buffer_span, u32 offset) {
	PASSERT(m_status == Status::frame_running);
	vkCmdDispatchIndirect(m_cur_cmd_buffer, buffer_span.buffer(),
						  buffer_span.byteOffset() + offset);
}

void VulkanCommandQueue::barrier(VPipeStages src, VPipeStages dst, VAccessFlags mem_src,
								 VAccessFlags mem_dst) {
	if(mem_src || mem_dst) {
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = mem_src.bits;
		barrier.dstAccessMask = mem_dst.bits;

		vkCmdPipelineBarrier(m_cur_cmd_buffer, toVk(src), toVk(dst), 0, 1, &barrier, 0, nullptr, 0,
							 nullptr);
	} else {
		vkCmdPipelineBarrier(m_cur_cmd_buffer, toVk(src), toVk(dst), 0, 0, nullptr, 0, nullptr, 0,
							 nullptr);
	}
}

void VulkanCommandQueue::fullBarrier() {
	barrier(VPipeStage::all_commands, VPipeStage::all_graphics,
			VAccess::memory_read | VAccess::memory_write,
			VAccess::memory_read | VAccess::memory_write);
}

uint VulkanCommandQueue::timestampQuery() {
	PASSERT(m_status == Status::frame_running);
	auto &frame = m_frames[m_swap_index];
	uint index = frame.query_count++;
	uint pool_id = index >> query_pool_shift;
	if(pool_id >= frame.query_pools.size()) {
		VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		ci.queryCount = query_pool_size;
		ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		VkQueryPool handle = nullptr;
		FWK_VK_CALL(vkCreateQueryPool, m_device_handle, &ci, nullptr, &handle);
		vkCmdResetQueryPool(m_cur_cmd_buffer, handle, 0, query_pool_size);
		frame.query_pools.emplace_back(handle);
	}

	vkCmdWriteTimestamp(m_cur_cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						frame.query_pools[pool_id], index & (query_pool_size - 1));
	return index;
}

void VulkanCommandQueue::perfTimestampQuery(uint sample_id) {
	auto query_id = timestampQuery();
	auto &frame = m_frames[m_swap_index];
	frame.perf_queries.emplace_back(sample_id, query_id);
}

// -------------------------------------------------------------------------------------------
// ----------  Upload commands ---------------------------------------------------------------

Ex<VBufferSpan<char>> VulkanCommandQueue::upload(VBufferSpan<char> dst, CSpan<char> src) {
	if(src.empty())
		return dst;
	DASSERT_LE(src.size(), dst.size());

	auto &mem_mgr = m_device.memory();
	auto mem_block = dst.buffer()->memoryBlock();
	if(canBeMapped(mem_block.id.domain())) {
		mem_block.offset += dst.byteOffset();
		// TODO: better checks
		mem_block.size = min<u32>(mem_block.size - dst.byteOffset(), src.size());
		fwk::copy(mem_mgr.writeAccessMemory(mem_block), src);
	} else {
		auto staging_buffer = EX_PASS(VulkanBuffer::create(
			m_device, src.size(), VBufferUsage::transfer_src, VMemoryUsage::host));
		auto mem_block = staging_buffer->memoryBlock();
		fwk::copy(mem_mgr.writeAccessMemory(mem_block), src);
		copy(dst, staging_buffer);
		m_staging_buffers.emplace_back(move(staging_buffer));
	}

	return VBufferSpan{dst.buffer(), dst.byteOffset() + src.size(), dst.size() - src.size()};
}
}
