// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef CASE_TYPE
#define CASE_TYPE(...)
#endif

// UpperCaseName; vulkan handle name; VTypeId enum member name
CASE_TYPE(Buffer, VkBuffer, buffer)
CASE_TYPE(Framebuffer, VkFramebuffer, framebuffer)
CASE_TYPE(Image, VkImage, image)
CASE_TYPE(ImageView, VkImageView, image_view)
CASE_TYPE(Pipeline, VkPipeline, pipeline)
CASE_TYPE(PipelineLayout, VkPipelineLayout, pipeline_layout)
CASE_TYPE(RenderPass, VkRenderPass, render_pass)
CASE_TYPE(ShaderModule, VkShaderModule, shader_module)
CASE_TYPE(SwapChain, VkSwapchainKHR, swap_chain)
CASE_TYPE(Sampler, VkSampler, sampler)

#undef CASE_TYPE
