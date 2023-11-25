// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <cstddef>
#include <limits>

namespace fwk {

// Either returns valid pointer or fails on FATAL
// Default new[] and delete[] operators also use these functions.
void *allocate(size_t size);
void *allocate(size_t size, size_t alignment);
void deallocate(void *);

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

	template <class Other> struct rebind {
		using other = SimpleAllocator<Other>;
	};

	void deallocate(T *ptr, size_t) { deallocateBytes(ptr); }
	template <class U> bool operator==(const SimpleAllocator<U> &rhs) const { return true; }
	template <class Other> bool operator==(const Other &rhs) const { return false; }
};
}
