// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_render_graph.h"

#include "fwk/gfx/image.h"
#include "fwk/index_range.h"
#include "fwk/vulkan/vulkan_buffer.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_memory.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_swap_chain.h"

// TODO: what should we do when error happens in begin/end frame?
// TODO: proprly handle multiple queues
// TODO: proprly differentiate between graphics, present & compute queues

namespace fwk {
VulkanRenderGraph::VulkanRenderGraph(VDeviceRef device)
	: m_device(*device), m_device_handle(device){};
VulkanRenderGraph::~VulkanRenderGraph() = default;

Ex<PVRenderPass> VulkanRenderGraph::createRenderPass(VDeviceRef device, PVSwapChain swap_chain) {
	auto sc_image = swap_chain->imageViews().front()->image();
	auto extent = sc_image->extent();

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = sc_image->format();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	return VulkanRenderPass::create(device, cspan(&colorAttachment, 1), cspan(&subpass, 1),
									cspan(&dependency, 1));
}

Ex<void> VulkanRenderGraph::initialize(VDeviceRef device, PVSwapChain swap_chain) {
	m_swap_chain = swap_chain;
	m_render_pass = EX_PASS(createRenderPass(device, swap_chain));
	auto queue_family = device->queues().front().second;

	auto image_views = m_swap_chain->imageViews();
	m_framebuffers.reserve(image_views.size());
	for(auto &image_view : image_views)
		m_framebuffers.emplace_back(
			EX_PASS(VulkanFramebuffer::create(device, cspan(&image_view, 1), m_render_pass)));

	m_command_pool = EX_PASS(device->createCommandPool(
		queue_family, VCommandPoolFlag::reset_command | VCommandPoolFlag::transient));
	for(auto &frame : m_frames) {
		frame.command_buffer = EX_PASS(m_command_pool->allocBuffer());
		frame.image_available_sem = EX_PASS(device->createSemaphore());
		frame.render_finished_sem = EX_PASS(device->createSemaphore());
		frame.in_flight_fence = EX_PASS(device->createFence(true));
	}

	return {};
}

void VulkanRenderGraph::initFrameAllocator(u64 base_size) {
	DASSERT(!m_frame_allocator);
	bool temp_available = m_device.isAvailable(VMemoryDomain::temporary);
	auto mem_domain = temp_available ? VMemoryDomain::temporary : VMemoryDomain::host;
	m_frame_allocator.emplace(m_device.ref(), mem_domain, base_size);
}

Ex<void> VulkanRenderGraph::beginFrame() {
	DASSERT(!m_frame_in_progress);
	auto &frame = m_frames[m_frame_index];

	VkFence fences[] = {frame.in_flight_fence};
	vkWaitForFences(m_device_handle, 1, fences, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device_handle, 1, fences);

	uint32_t image_index;
	if(vkAcquireNextImageKHR(m_device_handle, m_swap_chain, UINT64_MAX, frame.image_available_sem,
							 VK_NULL_HANDLE, &image_index) != VK_SUCCESS)
		FATAL("vkAcquireNextImageKHR failed");

	m_image_index = image_index;

	vkResetCommandBuffer(frame.command_buffer, 0);
	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	bi.pInheritanceInfo = nullptr; // Optional
	if(vkBeginCommandBuffer(frame.command_buffer, &bi) != VK_SUCCESS)
		return ERROR("vkBeginCommandBuffer failed");
	m_frame_in_progress = true;
	if(m_frame_allocator)
		m_frame_allocator->startFrame(m_frame_index);
	return {};
}

Ex<void> VulkanRenderGraph::finishFrame() {
	DASSERT(m_frame_in_progress);
	flushCommands();
	m_frame_in_progress = false;

	auto &frame = m_frames[m_frame_index];
	if(vkEndCommandBuffer(frame.command_buffer) != VK_SUCCESS)
		return ERROR("vkEndCommandBuffer failed");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {frame.image_available_sem};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	VkCommandBuffer cmd_bufs[1] = {frame.command_buffer};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = cmd_bufs;

	VkSemaphore signalSemaphores[] = {frame.render_finished_sem};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	auto queues = m_device.queues();
	auto graphicsQueue = queues.front().first;
	auto presentQueue = queues.back().first;
	if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.in_flight_fence) != VK_SUCCESS)
		return ERROR("vkQueueSubmit failed");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = {m_swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_image_index;
	presentInfo.pResults = nullptr; // Optional

	if(vkQueuePresentKHR(presentQueue, &presentInfo) != VK_SUCCESS)
		return ERROR("vkQueuePresentKHR failed");
	m_frame_index = (m_frame_index + 1) % num_swap_frames;

	// TODO: staging buffers destruction should be hooked to fence
	m_staging_buffers.clear();
	return {};
}

void VulkanRenderGraph::enqueue(Command cmd) { m_commands.emplace_back(move(cmd)); }

Ex<void> VulkanRenderGraph::enqueue(CmdUpload cmd) {
	if(cmd.data.empty())
		return {};

	// TODO: add weak DeviceRef for passing into functions ?
	// Maybe instead create functions should all be in device?
	auto staging_buffer =
		EX_PASS(VulkanBuffer::create(m_device.ref(), cmd.data.size(), VBufferUsage::transfer_src));
	auto staging_mem_req = staging_buffer->memoryRequirements();
	auto staging_mem_flags = VMemoryFlag::host_visible | VMemoryFlag::host_cached;
	auto staging_memory = EX_PASS(m_device.allocDeviceMemory(
		staging_mem_req.size, staging_mem_req.memoryTypeBits, staging_mem_flags));

	EXPECT(staging_buffer->bindMemory(staging_memory));

	staging_buffer->upload(cmd.data);
	VkBufferCopy copy_params{
		.srcOffset = 0, .dstOffset = cmd.offset, .size = (VkDeviceSize)cmd.data.size()};
	m_commands.emplace_back(
		CmdCopy{.src = staging_buffer, .dst = cmd.dst, .regions = {{copy_params}}});
	m_staging_buffers.emplace_back(move(staging_buffer));
	// TODO: don't use staging buffer if:
	// cmd.buffer is host_visible
	// cmd.buffer isn't being used yet?

	return {};
}

Ex<void> VulkanRenderGraph::enqueue(CmdUploadImage cmd) {
	if(cmd.image.empty())
		return {};

	int data_size = cmd.image.data().size() * sizeof(IColor);

	// TODO: add weak DeviceRef for passing into functions ?
	// Maybe instead create functions should all be in device?
	auto staging_buffer =
		EX_PASS(VulkanBuffer::create(m_device.ref(), data_size, VBufferUsage::transfer_src));
	auto staging_mem_req = staging_buffer->memoryRequirements();
	auto staging_mem_flags = VMemoryFlag::host_visible | VMemoryFlag::host_cached;
	auto staging_memory = EX_PASS(m_device.allocDeviceMemory(
		staging_mem_req.size, staging_mem_req.memoryTypeBits, staging_mem_flags));

	EXPECT(staging_buffer->bindMemory(staging_memory));

	staging_buffer->upload(cmd.image.data());
	m_commands.emplace_back(CmdCopyImage{.src = staging_buffer, .dst = cmd.dst});
	m_staging_buffers.emplace_back(move(staging_buffer));
	// TODO: don't use staging buffer if:
	// cmd.buffer is host_visible
	// cmd.buffer isn't being used yet?

	return {};
}

void VulkanRenderGraph::flushCommands() {
	DASSERT(m_frame_in_progress);
	FrameContext ctx{m_frames[m_frame_index].command_buffer, VCommandId(0)};

	for(int i : intRange(m_commands)) {
		auto &command = m_commands[i];
		ctx.cmd_id = VCommandId(i);
		command.visit([&ctx, this](auto &cmd) { perform(ctx, cmd); });
	}
	m_commands.clear();
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindDescriptorSet &cmd) {
	VkDescriptorSet handle = cmd.set->handle;
	vkCmdBindDescriptorSets(ctx.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.set->layout,
							cmd.set->layout_index, 1, &handle, 0, nullptr);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindIndexBuffer &cmd) {
	vkCmdBindIndexBuffer(ctx.cmd_buffer, cmd.buffer, 0, VkIndexType::VK_INDEX_TYPE_UINT32);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindVertexBuffers &cmd) {
	static constexpr int max_bindings = 32;
	VkBuffer buffers[max_bindings];
	DASSERT(cmd.buffers.size() <= max_bindings);
	for(int i : intRange(cmd.buffers))
		buffers[i] = cmd.buffers[i];
	vkCmdBindVertexBuffers(ctx.cmd_buffer, cmd.first_binding, cmd.buffers.size(), buffers,
						   cmd.offsets.data());
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBindPipeline &cmd) {
	vkCmdBindPipeline(ctx.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdDraw &cmd) {
	vkCmdDraw(ctx.cmd_buffer, cmd.num_vertices, cmd.num_instances, cmd.first_vertex,
			  cmd.first_instance);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdCopy &cmd) {
	vkCmdCopyBuffer(ctx.cmd_buffer, cmd.src, cmd.dst, cmd.regions.size(), cmd.regions.data());
}

static void transitionImageLayout(VkCommandBuffer cmd_buffer, VkImage image, VkFormat format,
								  VImageLayout old_layout, VImageLayout new_layout) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = toVk(old_layout);
	barrier.newLayout = toVk(new_layout);
	barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
	barrier.srcAccessMask = 0; // TODO
	barrier.dstAccessMask = 0; // TODO

	VkPipelineStageFlags src_stage, dst_stage;
	// TODO: properly handle access masks & stages
	if(old_layout == VImageLayout::undefined && new_layout == VImageLayout::transfer_dst) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if(old_layout == VImageLayout::transfer_dst &&
			  new_layout == VImageLayout::shader_read_only) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		FATAL("Unsupported layout transition: %s -> %s", toString(old_layout),
			  toString(new_layout));
	}

	vkCmdPipelineBarrier(cmd_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdCopyImage &cmd) {
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	// TODO: do it properly
	auto dst_extent = cmd.dst->extent();
	region.bufferImageHeight = 0;
	region.bufferRowLength = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageExtent = {dst_extent.width, dst_extent.height, 1};

	auto copy_layout = VImageLayout::transfer_dst;
	auto dst_layout = cmd.dst_layout.orElse(VImageLayout::shader_read_only);
	transitionImageLayout(ctx.cmd_buffer, cmd.dst, cmd.dst->format(), cmd.dst->m_last_layout,
						  copy_layout);
	vkCmdCopyBufferToImage(ctx.cmd_buffer, cmd.src, cmd.dst, toVk(copy_layout), 1, &region);

	// TODO: do this transition right before we're going to do something with this texture?
	transitionImageLayout(ctx.cmd_buffer, cmd.dst, cmd.dst->format(), copy_layout, dst_layout);
	cmd.dst->m_last_layout = dst_layout;
}

void VulkanRenderGraph::perform(FrameContext &ctx, const CmdBeginRenderPass &cmd) {
	VkRenderPassBeginInfo bi{};
	auto framebuffer = m_framebuffers[m_image_index];

	bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	bi.renderPass = cmd.render_pass;
	if(cmd.render_area)
		bi.renderArea = toVkRect(*cmd.render_area);
	else
		bi.renderArea.extent = framebuffer->extent();
	bi.clearValueCount = cmd.clear_values.size();
	bi.pClearValues = cmd.clear_values.data();
	bi.framebuffer = framebuffer;
	vkCmdBeginRenderPass(ctx.cmd_buffer, &bi, VK_SUBPASS_CONTENTS_INLINE);
}
void VulkanRenderGraph::perform(FrameContext &ctx, const CmdEndRenderPass &cmd) {
	vkCmdEndRenderPass(ctx.cmd_buffer);
}
}