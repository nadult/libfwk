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
#include "fwk/vulkan/vulkan_render_pass.h"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

// TODO: add function for clearing labeled downloads

namespace fwk {

VulkanCommandQueue::VulkanCommandQueue(VulkanDevice &device)
	: m_device(device), m_device_handle(device) {}

VulkanCommandQueue::~VulkanCommandQueue() {
	DASSERT(m_status != Status::frame_running);
	if(!m_device_handle)
		return;
	vkDeviceWaitIdle(m_device_handle);
	auto destroy_syncs = [&](CommandBufferInfo &commands) {
		for(auto &semaphore : commands.semaphores)
			vkDestroySemaphore(m_device_handle, semaphore, nullptr);
		vkDestroyFence(m_device_handle, commands.fence, nullptr);
	};

	for(auto &commands : m_recycled_commands)
		destroy_syncs(commands);
	for(auto &swap_frame : m_swap_frames) {
		for(auto &commands : swap_frame.previous_commands)
			destroy_syncs(commands);
		for(auto &commands : swap_frame.commands)
			destroy_syncs(commands);
		for(auto pool : swap_frame.query_pools)
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
	m_timestamp_period = m_device.physInfo().properties.limits.timestampPeriod;
	beginCommands();
	return {};
}

auto VulkanCommandQueue::acquireCommands() -> CommandBufferInfo {
	if(m_recycled_commands) {
		auto result = m_recycled_commands.back();
		m_recycled_commands.pop_back();
		return result;
	}

	return {allocVkCommandBuffer(m_device, m_command_pool),
			{createVkSemaphore(m_device), createVkSemaphore(m_device)},
			createVkFence(m_device)};
}

void VulkanCommandQueue::recycleCommands(CommandBufferInfo buffer) {
	vkResetCommandBuffer(buffer.buffer, 0);
	vkResetFences(m_device_handle, 1, &buffer.fence);
	m_recycled_commands.emplace_back(buffer);
}

void VulkanCommandQueue::beginCommands() {
	DASSERT(m_cur_cmd_buffer == nullptr);
	VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	auto &swap_frame = m_swap_frames[m_swap_index];
	swap_frame.commands.emplace_back(acquireCommands());
	m_cur_cmd_buffer = swap_frame.commands.back().buffer;
	FWK_VK_CALL(vkBeginCommandBuffer, m_cur_cmd_buffer, &bi);
}

VkSemaphore VulkanCommandQueue::submitCommands(VkSemaphore wait_semaphore, VPipeStages wait_stage,
											   bool need_signal_semaphore) {
	auto &swap_frame = m_swap_frames[m_swap_index];
	DASSERT(!swap_frame.commands.empty());
	auto &current = swap_frame.commands.back();
	DASSERT(current.buffer == m_cur_cmd_buffer);
	FWK_VK_CALL(vkEndCommandBuffer, current.buffer);
	m_cur_cmd_buffer = nullptr;

	VkSemaphore wait_semaphores[2];
	VkPipelineStageFlags wait_stages[2];
	int num_semaphore_waits = 0;
	if(wait_semaphore) {
		wait_semaphores[num_semaphore_waits] = wait_semaphore;
		wait_stages[num_semaphore_waits] = toVk(wait_stage);
		num_semaphore_waits++;
	}
	if(m_last_submitted_semaphore) {
		wait_semaphores[num_semaphore_waits] = m_last_submitted_semaphore;
		wait_stages[num_semaphore_waits] = toVk(VPipeStage::top);
		num_semaphore_waits++;
	}

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.waitSemaphoreCount = num_semaphore_waits;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &current.buffer;
	submit_info.pSignalSemaphores = current.semaphores;
	submit_info.signalSemaphoreCount = need_signal_semaphore ? 2 : 1;

	FWK_VK_CALL(vkQueueSubmit, m_queue.handle, 1, &submit_info, current.fence);
	m_last_submitted_semaphore = current.semaphores[0];
	return need_signal_semaphore ? current.semaphores[1] : nullptr;
}

void VulkanCommandQueue::submit() {
	submitCommands(nullptr, none, false);
	beginCommands();
}

void VulkanCommandQueue::finish() {
	auto &swap_frame = m_swap_frames[m_swap_index];
	if(swap_frame.num_waited_fences >= swap_frame.commands.size())
		return;
	// We only need to wait for the fence of the last submitted command buffer,
	// because each command buffer waits for the prevous one to finish
	auto last_fence = swap_frame.commands.back().fence;
	vkWaitForFences(m_device_handle, 1, &last_fence, VK_TRUE, UINT64_MAX);
	swap_frame.num_waited_fences = swap_frame.commands.size();
}

void VulkanCommandQueue::waitForSwapFrameAvailable() {
	DASSERT(m_status != Status::frame_running);
	auto &swap_frame = m_swap_frames[m_swap_index];
	if(swap_frame.previous_commands) {
		auto &previous_fence = swap_frame.previous_commands.back().fence;
		vkWaitForFences(m_device_handle, 1, &previous_fence, VK_TRUE, UINT64_MAX);
		for(auto &command : swap_frame.previous_commands)
			recycleCommands(command);
		swap_frame.previous_commands.clear();
	}
}

void VulkanCommandQueue::beginFrame() {
	DASSERT(m_status != Status::frame_running);
	auto &swap_frame = m_swap_frames[m_swap_index];

	for(auto &download : m_downloads)
		if(download.frame_index + VulkanLimits::num_swap_frames <= m_frame_index)
			download.is_ready = true;
	m_status = Status::frame_running;

	if(swap_frame.query_count > 0) {
		PodVector<u64> new_data(swap_frame.query_count);
		uint cur_count = 0;
		for(auto pool : swap_frame.query_pools) {
			uint pool_count = min(swap_frame.query_count - cur_count, query_pool_size);
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
		new_data.unsafeSwap(swap_frame.query_results);
	}
	swap_frame.query_count = 0;

	auto *perf_tc = perf::ThreadContext::current();
	if(swap_frame.perf_queries) {
		auto first_value = swap_frame.query_results[0];
		if(perf_tc) {
			auto sample_values =
				transform(swap_frame.perf_queries, [&](Pair<uint, uint> sample_query_id) {
					auto value = swap_frame.query_results[sample_query_id.second] - first_value;
					value *= m_timestamp_period;
					return pair{sample_query_id.first, u64(value)};
				});
			perf_tc->resolveGpuScopes(swap_frame.perf_frame_id, sample_values);
		}
		swap_frame.perf_queries.clear();
	}
	swap_frame.perf_frame_id = perf_tc ? perf_tc->frameId() : -1;

	auto first_query = timestampQuery();
	PASSERT(first_query == 0);
}

VkSemaphore VulkanCommandQueue::finishFrame(VkSemaphore image_available_semaphore) {
	DASSERT(m_status == Status::frame_running);
	auto &swap_frame = m_swap_frames[m_swap_index];
	auto signalled_semaphore =
		submitCommands(image_available_semaphore, VPipeStage::color_att_output, true);
	DASSERT(!swap_frame.previous_commands);
	swap_frame.previous_commands = std::move(swap_frame.commands);
	swap_frame.num_waited_fences = 0;
	m_status = Status::frame_finished;

	m_swap_index = (m_swap_index + 1) % VulkanLimits::num_swap_frames;
	m_frame_index++;
	beginCommands();

	m_last_pipeline_layout.reset();
	m_last_bind_point = VBindPoint::graphics;
	m_last_viewport = {};
	return signalled_semaphore;
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
		if(i64(frame_index) == frame_indices[i]) {
			uint idx = (i + m_swap_index) % num_swap_frames;
			return m_swap_frames[idx].query_results;
		}
	return {};
}

// -------------------------------------------------------------------------------------------
// ------------------------------------  Commands --------------------------------------------

void VulkanCommandQueue::copy(VBufferSpan<char> dst, VBufferSpan<char> src) {
	DASSERT_LE(src.size(), dst.size());
	VkBufferCopy copy_params{src.byteOffset(), dst.byteOffset(), VkDeviceSize(src.byteSize())};
	vkCmdCopyBuffer(m_cur_cmd_buffer, src.buffer(), dst.buffer(), 1, &copy_params);
}

void VulkanCommandQueue::copy(PVImage dst, VBufferSpan<> src, Maybe<IBox> box, int dst_mip_level,
							  VImageLayout dst_layout) {
	auto mip_size = dst->mipSize(dst_mip_level);
	if(!box) {
		box = IBox{int3(0), mip_size};
	} else {
		auto box_min = box->min(), box_max = box->max();
		DASSERT(box_min.x >= 0 && box_min.y >= 0 && box_min.z >= 0);
		DASSERT(box_max.x <= mip_size.x && box_max.y <= mip_size.y && box_max.z <= mip_size.z);
	}

	VkBufferImageCopy region{};
	region.bufferOffset = src.byteOffset();
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = dst_mip_level;
	auto box_size = box->size(), box_offset = box->min();
	region.imageExtent = {uint(box_size.x), uint(box_size.y), uint(box_size.z)};
	region.imageOffset = {box_offset.x, box_offset.y, box_offset.z};

	auto copy_layout =
		dst_layout == VImageLayout::general ? VImageLayout::general : VImageLayout::transfer_dst;
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

void VulkanCommandQueue::setPushConstants(u32 offset, VShaderStages stages, CSpan<char> data) {
	vkCmdPushConstants(m_cur_cmd_buffer, m_last_pipeline_layout, toVk(stages), offset, data.size(),
					   data.data());
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

	PASSERT(!m_current_render_pass && !m_current_framebuffer);
	m_current_render_pass = render_pass;
	m_current_framebuffer = framebuffer;
	updateAttachmentsLayouts(false);
}

void VulkanCommandQueue::updateAttachmentsLayouts(bool final) {
	PASSERT(m_current_render_pass && m_current_framebuffer);
	auto attachments = m_current_framebuffer->attachments();
	auto rp_attachments = m_current_render_pass->attachments();
	PASSERT(attachments.size() == rp_attachments.size());
	for(int i = 0; i < attachments.size(); i++) {
		auto image = attachments[i]->image();
		auto sync = rp_attachments[i].sync();
		auto layout = final ? sync.finalLayout() : sync.initialLayout();
		image->setLayout(layout, 0);
	}
}

void VulkanCommandQueue::beginRenderPass(CSpan<PVImageView> attachments, PVRenderPass render_pass,
										 Maybe<IRect> render_area,
										 CSpan<VClearValue> clear_values) {
	auto framebuffer = m_device.getFramebuffer(attachments, render_pass);
	beginRenderPass(framebuffer, render_pass, render_area, clear_values);
}

void VulkanCommandQueue::endRenderPass() {
	PASSERT(m_status == Status::frame_running);
	vkCmdEndRenderPass(m_cur_cmd_buffer);
	updateAttachmentsLayouts(true);
	m_current_render_pass = {};
	m_current_framebuffer = {};
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
	barrier(VPipeStage::all_commands, VPipeStage::all_commands,
			VAccess::memory_read | VAccess::memory_write,
			VAccess::memory_read | VAccess::memory_write);
}

void VulkanCommandQueue::clearColor(PVImage image, VClearValue color) {
	VkImageSubresourceRange range{};
	range.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = range.layerCount = 1;
	vkCmdClearColorImage(m_cur_cmd_buffer, image, toVk(image->layout(0)),
						 reinterpret_cast<VkClearColorValue *>(&color), 1, &range);
}

uint VulkanCommandQueue::timestampQuery() {
	PASSERT(m_status == Status::frame_running);
	auto &swap_frame = m_swap_frames[m_swap_index];
	uint index = swap_frame.query_count++;
	int pool_id = int(index >> query_pool_shift);
	if(pool_id >= swap_frame.query_pools.size()) {
		VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		ci.queryCount = query_pool_size;
		ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
		VkQueryPool handle = nullptr;
		FWK_VK_CALL(vkCreateQueryPool, m_device_handle, &ci, nullptr, &handle);
		vkCmdResetQueryPool(m_cur_cmd_buffer, handle, 0, query_pool_size);
		swap_frame.query_pools.emplace_back(handle);
	}

	vkCmdWriteTimestamp(m_cur_cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						swap_frame.query_pools[pool_id], index & (query_pool_size - 1));
	return index;
}

void VulkanCommandQueue::perfTimestampQuery(uint sample_id) {
	auto query_id = timestampQuery();
	auto &swap_frame = m_swap_frames[m_swap_index];
	swap_frame.perf_queries.emplace_back(sample_id, query_id);
}

// -------------------------------------------------------------------------------------------
// ------------------------------  Download commands -----------------------------------------

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

Ex<VDownloadId> VulkanCommandQueue::download(VBufferSpan<char> src) {
	PASSERT(m_status == Status::frame_running);
	auto buffer = EX_PASS(
		VulkanBuffer::create(m_device, src.size(), VBufferUsage::transfer_dst, VMemoryUsage::host));
	copy(buffer, src);
	auto id = m_downloads.emplace(std::move(buffer), m_frame_index);
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
		current->last_frame_index = m_frame_index;
	}

	PodVector<char> out;
	while(current->ids) {
		auto id = current->ids.front();
		out = retrieve<char>(id);
		if(!out)
			break;
		current->ids.erase(current->ids.begin());
	}

	PASSERT(m_frame_index >= current->last_frame_index);
	u64 frame_counter = m_frame_index - current->last_frame_index;
	if(skip_frames && frame_counter % (skip_frames + 1) == 0) {
		current->ids.emplace_back(EX_PASS(download(src)));
		current->last_frame_index = m_frame_index;
	}

	return out;
}

}
