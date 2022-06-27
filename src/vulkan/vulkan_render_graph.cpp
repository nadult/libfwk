// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/vulkan/vulkan_render_graph.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_framebuffer.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_swap_chain.h"

#include <vulkan/vulkan.h>

namespace fwk {
VulkanRenderGraph::VulkanRenderGraph(VDeviceRef device) : m_device(device){};
VulkanRenderGraph::~VulkanRenderGraph() = default;

Ex<PVRenderGraph> VulkanRenderGraph::create(VDeviceRef device, PVSwapChain swap_chain) {
	PVRenderGraph out;
	out.emplace(device);
	EXPECT(out->initialize(device, swap_chain));
	return out;
}

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

	m_command_pool =
		EX_PASS(device->createCommandPool(queue_family, CommandPoolFlag::reset_command));
	for(auto &frame : m_frames) {
		frame.command_buffer = EX_PASS(m_command_pool->allocBuffer());
		frame.image_available_sem = EX_PASS(device->createSemaphore());
		frame.render_finished_sem = EX_PASS(device->createSemaphore());
		frame.in_flight_fence = EX_PASS(device->createFence(true));
	}

	return {};
}

Pair<PVCommandBuffer, PVFramebuffer> VulkanRenderGraph::beginFrame() {
	auto &frame = m_frames[m_frame_index];

	VkFence fences[] = {frame.in_flight_fence};
	vkWaitForFences(m_device, 1, fences, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device, 1, fences);

	uint32_t image_index;
	if(vkAcquireNextImageKHR(m_device, m_swap_chain, UINT64_MAX, frame.image_available_sem,
							 VK_NULL_HANDLE, &image_index) != VK_SUCCESS)
		FATAL("vkAcquireNextImageKHR failed");

	m_image_index = image_index;
	return {frame.command_buffer, m_framebuffers[image_index]};
}

void VulkanRenderGraph::finishFrame() {
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	auto &frame = m_frames[m_frame_index];
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

	auto queues = m_device->queues();
	auto graphicsQueue = queues.front().first;
	auto presentQueue = queues.back().first;
	if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.in_flight_fence) != VK_SUCCESS)
		FWK_FATAL("queue submit fail");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = {m_swap_chain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_image_index;
	presentInfo.pResults = nullptr; // Optional

	vkQueuePresentKHR(presentQueue, &presentInfo);
	m_frame_index = (m_frame_index + 1) % num_swap_frames;
}

}