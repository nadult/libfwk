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

// fwk class type, vulkan handle type; enum type-id (VTypeId)
CASE_WRAPPED_TYPE(VulkanBuffer, VkBuffer, buffer)
CASE_LIGHT_TYPE(None, VkFence, fence)
CASE_LIGHT_TYPE(None, VkSemaphore, semaphore)

#undef CASE_TYPE
#undef CASE_LIGHT_TYPE
#undef CASE_WRAPPED_TYPE
