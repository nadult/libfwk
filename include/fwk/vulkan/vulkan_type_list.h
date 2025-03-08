// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef CASE_TYPE
#define CASE_TYPE(...)
#endif

// FullClassName; vulkan handle name; VTypeId enum member name
CASE_TYPE(VulkanBuffer, VkBuffer, buffer)
CASE_TYPE(VulkanFramebuffer, VkFramebuffer, framebuffer)
CASE_TYPE(VulkanImage, VkImage, image)
CASE_TYPE(VulkanImageView, VkImageView, image_view)
CASE_TYPE(VulkanPipeline, VkPipeline, pipeline)
CASE_TYPE(VulkanPipelineLayout, VkPipelineLayout, pipeline_layout)
CASE_TYPE(VulkanRenderPass, VkRenderPass, render_pass)
CASE_TYPE(VulkanShaderModule, VkShaderModule, shader_module)
CASE_TYPE(VulkanSwapChain, VkSwapchainKHR, swap_chain)
CASE_TYPE(VulkanSampler, VkSampler, sampler)
CASE_TYPE(VulkanAccelStruct, VkAccelerationStructureKHR, accel_struct)

#undef CASE_TYPE
