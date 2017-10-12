// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <cstddef>
#include <limits>

namespace fwk {

namespace detail {
	using AllocFunc = void *(*)(size_t);
	using FreeFunc = void (*)(void *);
	using AlignedAllocFunc = void *(*)(size_t, size_t);

	// TODO: thread local ptrs ?
	extern AllocFunc g_alloc;
	extern AlignedAllocFunc g_aligned_alloc;
	extern FreeFunc g_free;
}

// Either returns valid pointer or fails on FATAL
inline void *allocate(size_t size) { return detail::g_alloc(size); }
inline void *allocate(size_t size, size_t alignment) {
	return detail::g_aligned_alloc(size, alignment);
}
inline void deallocate(void *ptr) { return detail::g_free(ptr); }

class SimpleAllocatorBase {
  public:
	void *allocateBytes(size_t count);
	void deallocateBytes(void *ptr);
};

// TODO: in multithreaded env consider: tbb::scalable_allocator
template <class T> class SimpleAllocator : public SimpleAllocatorBase {
  public:
	using value_type = T;

	SimpleAllocator() = default;
	template <typename U> SimpleAllocator(const SimpleAllocator<U> &other) {}

	T *allocate(size_t count) { return static_cast<T *>(allocateBytes(count * sizeof(T))); }

	size_t max_size() const { return std::numeric_limits<size_t>::max() / sizeof(T); }

	template <class Other> struct rebind { using other = SimpleAllocator<Other>; };

	void deallocate(T *ptr, size_t) { deallocateBytes(ptr); }
	template <class U> bool operator==(const SimpleAllocator<U> &rhs) const { return true; }
	template <class Other> bool operator==(const Other &rhs) const { return false; }
};
}
