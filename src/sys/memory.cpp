// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/memory.h"

#include "fwk/sys_base.h"
#include <cstdlib>
#include <new>

namespace fwk {

namespace detail {
	void *falloc(size_t size) {
		auto ptr = malloc(size);
		if(!ptr)
			FATAL("Allocation error: failed to allocate %llu bytes", (unsigned long long)size);
		return ptr;
	}

	void *falloc(size_t size, size_t alignment) {
		DASSERT((alignment & (alignment - 1)) == 0);
#ifdef FWK_TARGET_MINGW
		auto ptr = _aligned_malloc(size, alignment);
#else
		auto ptr = aligned_alloc(alignment, size);
#endif
		if(!ptr)
			FATAL("Allocation error: failed to allocate %lld bytes (aligned to: %lld)",
				  (long long)size, (long long)alignment);
		return ptr;
	}

	void ffree(void *ptr) { return ::free(ptr); }

	AllocFunc g_alloc = falloc;
	AlignedAllocFunc g_aligned_alloc = falloc;
	FreeFunc g_free = ffree;
}

void *SimpleAllocatorBase::allocateBytes(size_t count) { return detail::g_alloc(count); }
void SimpleAllocatorBase::deallocateBytes(void *ptr) { fwk::detail::g_free(ptr); }
}

void *operator new(std::size_t count) { return fwk::detail::g_alloc(count); }
void *operator new[](std::size_t count) { return fwk::detail::g_alloc(count); }

void operator delete(void *ptr) noexcept { fwk::detail::g_free(ptr); }
void operator delete[](void *ptr) noexcept { fwk::detail::g_free(ptr); }

void operator delete(void *ptr, std::size_t sz) { fwk::detail::g_free(ptr); }
void operator delete[](void *ptr, std::size_t sz) { fwk::detail::g_free(ptr); }

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
