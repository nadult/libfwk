// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/memory.h"

#include "fwk/math_base.h"
#include "fwk/sys_base.h"
#include <cstdlib>
#include <new>

#ifdef FWK_PLATFORM_HTML5
void *aligned_alloc(size_t alignment, size_t size) {
	// TODO: do this properly; although it shouldnt matter on thiis platform
	return malloc(size);
}
#endif

namespace fwk {

void *allocate(size_t size) {
	auto ptr = malloc(size);
	if(!ptr)
		FWK_FATAL("Allocation error: failed to allocate %llu bytes", (unsigned long long)size);
	return ptr;
}

void *allocate(size_t size, size_t alignment) {
	DASSERT(isPowerOfTwo(alignment));
#if defined(FWK_PLATFORM_MINGW) || defined(FWK_PLATFORM_MSVC)
	auto ptr = _aligned_malloc(size, alignment);
#else
	auto ptr = aligned_alloc(alignment, size);
#endif
	if(!ptr)
		FWK_FATAL("Allocation error: failed to allocate %lld bytes (aligned to: %lld)", (llint)size,
				  (llint)alignment);
	return ptr;
}

void deallocate(void *ptr) { return ::free(ptr); }

void *SimpleAllocatorBase::allocateBytes(size_t count) { return allocate(count); }
void SimpleAllocatorBase::deallocateBytes(void *ptr) { deallocate(ptr); }
}

void *operator new(std::size_t count) { return fwk::allocate(count); }
void *operator new[](std::size_t count) { return fwk::allocate(count); }

void operator delete(void *ptr) noexcept { fwk::deallocate(ptr); }
void operator delete[](void *ptr) noexcept { fwk::deallocate(ptr); }

void operator delete(void *ptr, std::size_t sz) noexcept { fwk::deallocate(ptr); }
void operator delete[](void *ptr, std::size_t sz) noexcept { fwk::deallocate(ptr); }

// TODO: C++17 alignment versions
/*
void *operator new(std::size_t count, std::align_val_t al) {
	return fwk::detail::g_aligned_alloc(count, al);
}
void *operator new[](std::size_t count, std::align_val_t al) {
	return fwk::detail::g_aligned_alloc(count, al);
}

void operator delete(void *ptr, std::align_val_t al) { fwk::detail::g_free(ptr); }
void operator delete[](void *ptr, std::align_val_t al) { fwk::detail::g_free(ptr); }
void operator delete(void *ptr, std::size_t sz, std::align_val_t al) { fwk::detail::g_free(ptr); }
void operator delete[](void *ptr, std::size_t sz, std::align_val_t al) { fwk::detail::g_free(ptr); }
*/
