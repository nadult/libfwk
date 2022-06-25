// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef CASE_TYPE
#define CASE_TYPE(...)
#endif

// uppercase name; lowercase name (VTypeId member)
CASE_TYPE(Buffer, buffer)
CASE_TYPE(Image, image)
CASE_TYPE(ImageView, image_view)
CASE_TYPE(RenderPass, render_pass)
CASE_TYPE(Pipeline, pipeline)
CASE_TYPE(Framebuffer, framebuffer)
CASE_TYPE(CommandPool, command_pool)
CASE_TYPE(Fence, fence)
CASE_TYPE(Semaphore, semaphore)
CASE_TYPE(ShaderModule, shader_module)
CASE_TYPE(PipelineLayout, pipeline_layout)

#undef CASE_TYPE
