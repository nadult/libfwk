// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// Wrapped types besides Vk handle also have an additional wrapper
// with helper functions and maybe some members
#ifndef CASE_WRAPPED_TYPE
#define CASE_WRAPPED_TYPE CASE_TYPE
#endif

//Light types only have Vk handles
#ifndef CASE_LIGHT_TYPE
#define CASE_LIGHT_TYPE CASE_TYPE
#endif

#ifndef CASE_TYPE
#define CASE_TYPE(...)
#endif

// uppercase name; lowercase name (VTypeId member)
CASE_WRAPPED_TYPE(Buffer, buffer)
CASE_WRAPPED_TYPE(Image, image)
CASE_WRAPPED_TYPE(RenderPass, render_pass)
CASE_WRAPPED_TYPE(Pipeline, pipeline)
CASE_LIGHT_TYPE(Fence, fence)
CASE_LIGHT_TYPE(Semaphore, semaphore)
CASE_LIGHT_TYPE(ShaderModule, shader_module)
CASE_LIGHT_TYPE(PipelineLayout, pipeline_layout)

#undef CASE_TYPE
#undef CASE_LIGHT_TYPE
#undef CASE_WRAPPED_TYPE
