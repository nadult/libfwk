// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/base_vector.h"
#include "fwk/sys/memory.h"

// Full inlining of this doesn't make a big difference really:
// libfwk build: 33.8 sec vs 32.5, size: 56MB vs 53MB
// TODO: there is still space for improvement (perf-wise) here
// TODO: more aggressive inlining here improves perf
namespace fwk {

namespace detail {

	constexpr int pool_alloc_size = 128, pool_max_size = 32;
	static __thread char *t_pool_buf[pool_max_size];
	static __thread int t_pool_size = 0;

	// TODO: describe
	inline char *poolAlloc(size_t size) {
		using namespace detail;
		if(size > pool_alloc_size || !t_pool_size)
			return new char[size > pool_alloc_size ? size : pool_alloc_size];
		return t_pool_buf[--t_pool_size];
	}

	inline void poolFree(char *ptr, size_t size) {
		using namespace detail;
		if(size > pool_alloc_size || t_pool_size == pool_max_size)
			delete[] ptr;
		else
			t_pool_buf[t_pool_size++] = ptr;
	}
}

template <bool pa> void BaseVector<pa>::free(int obj_size) {
	if constexpr(pa)
		detail::poolFree(data, size_t(size) * obj_size);
	else
		fwk::deallocate(data);
}
template <bool pa> void BaseVector<pa>::alloc(int obj_size, int size_, int capacity_) {
	size = size_;
	capacity = capacity_;
	auto nbytes = size_t(capacity) * obj_size;
	if constexpr(pa)
		data = detail::poolAlloc(nbytes);
	else
		data = (char *)fwk::allocate(nbytes);
}

template <bool pa> void BaseVector<pa>::swap(BaseVector<pa> &rhs) {
	fwk::swap(data, rhs.data);
	fwk::swap(size, rhs.size);
	fwk::swap(capacity, rhs.capacity);
}

template <bool pa>
void BaseVector<pa>::reallocate(int obj_size, MoveDestroyFunc move_destroy_func, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector<pa> new_base;
	new_base.alloc(obj_size, size, new_capacity);
	move_destroy_func(new_base.data, data, size);
	swap(new_base);
}

template <bool pa> void BaseVector<pa>::grow(int obj_size, MoveDestroyFunc move_destroy_func) {
	reallocate(obj_size, move_destroy_func, vectorGrowCapacity(capacity, obj_size));
}

template <bool pa>
void BaseVector<pa>::resizePartial(int obj_size, DestroyFunc destroy_func,
								   MoveDestroyFunc move_destroy_func, int new_size) {
	PASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, vectorInsertCapacity(capacity, obj_size, new_size));

	if(size > new_size)
		destroy_func(data + size_t(obj_size) * new_size, size - new_size);
	size = new_size;
}

template <bool pa>
void BaseVector<pa>::assignPartial(int obj_size, DestroyFunc destroy_func, int new_size) {
	clear(destroy_func);
	if(new_size > capacity) {
		BaseVector<pa> new_base;
		new_base.alloc(obj_size, new_size, vectorInsertCapacity(capacity, obj_size, new_size));
		swap(new_base);
		return;
	}
	size = new_size;
}

template <bool pa>
void BaseVector<pa>::assign(int obj_size, DestroyFunc destroy_func, CopyFunc copy_func,
							const void *ptr, int new_size) {
	assignPartial(obj_size, destroy_func, new_size);
	copy_func(data, ptr, size);
}

template <bool pa>
void BaseVector<pa>::insertPartial(int obj_size, MoveDestroyFunc move_destroy_func, int index,
								   int count) {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, vectorInsertCapacity(capacity, obj_size, new_size));

	int move_count = size - index;
	if(move_count > 0)
		move_destroy_func(data + size_t(obj_size) * (index + count),
						  data + size_t(obj_size) * index, move_count);
	size = new_size;
}

template <bool pa>
void BaseVector<pa>::insert(int obj_size, MoveDestroyFunc move_destroy_func, CopyFunc copy_func,
							int index, const void *ptr, int count) {
	insertPartial(obj_size, move_destroy_func, index, count);
	copy_func(data + size_t(obj_size) * index, ptr, count);
}

template <bool pa> void BaseVector<pa>::clear(DestroyFunc destroy_func) {
	destroy_func(data, size);
	size = 0;
}

template <bool pa>
void BaseVector<pa>::erase(int obj_size, DestroyFunc destroy_func,
						   MoveDestroyFunc move_destroy_func, int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;

	destroy_func(data + size_t(obj_size) * index, count);
	move_destroy_func(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
					  move_count);
	size -= count;
}

template <bool pa> void BaseVector<pa>::reallocatePod(int obj_size, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector<pa> new_base;
	new_base.alloc(obj_size, size, new_capacity);
	memcpy(new_base.data, data, size_t(obj_size) * size);
	swap(new_base);
}

template <bool pa> void BaseVector<pa>::reservePod(int obj_size, int desired_capacity) {
	if(desired_capacity > capacity) {
		int new_capacity = vectorInsertCapacity(capacity, obj_size, desired_capacity);
		reallocatePod(obj_size, new_capacity);
	}
}

template <bool pa>
void BaseVector<pa>::reserve(int obj_size, MoveDestroyFunc func, int desired_capacity) {
	if(desired_capacity > capacity) {
		int new_capacity = vectorInsertCapacity(capacity, obj_size, desired_capacity);
		reallocate(obj_size, func, new_capacity);
	}
}

template <bool pa> void BaseVector<pa>::growPod(int obj_size) {
	int current = capacity;
	reallocatePod(obj_size, vectorGrowCapacity(capacity, obj_size));
}

template <bool pa> void BaseVector<pa>::resizePodPartial(int obj_size, int new_size) {
	PASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocatePod(obj_size, vectorInsertCapacity(capacity, obj_size, new_size));
	size = new_size;
}

template <bool pa> void BaseVector<pa>::assignPartialPod(int obj_size, int new_size) {
	clearPod();
	if(new_size > capacity) {
		BaseVector<pa> new_base;
		new_base.alloc(obj_size, new_size, vectorInsertCapacity(capacity, obj_size, new_size));
		swap(new_base);
		return;
	}
	size = new_size;
}

template <bool pa> void BaseVector<pa>::assignPod(int obj_size, const void *ptr, int new_size) {
	assignPartialPod(obj_size, new_size);
	memcpy(data, ptr, size_t(obj_size) * size);
}

template <bool pa> void BaseVector<pa>::insertPodPartial(int obj_size, int index, int count) {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocatePod(obj_size, vectorInsertCapacity(capacity, obj_size, new_size));

	int move_count = size - index;
	if(move_count > 0)
		memmove(data + size_t(obj_size) * (index + count), data + size_t(obj_size) * index,
				size_t(obj_size) * move_count);
	size = new_size;
}

template <bool pa>
void BaseVector<pa>::insertPod(int obj_size, int index, const void *ptr, int count) {
	insertPodPartial(obj_size, index, count);
	memcpy(data + size_t(obj_size) * index, ptr, size_t(obj_size) * count);
}

template <bool pa> void BaseVector<pa>::erasePod(int obj_size, int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;
	if(move_count > 0)
		memmove(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
				size_t(obj_size) * move_count);
	size -= count;
}

template <bool pa> void BaseVector<pa>::invalidIndex(int index) const {
	FWK_FATAL("Index %d out of range: <%d; %d)", index, 0, size);
}

template <bool pa> void BaseVector<pa>::invalidEmpty() const {
	FWK_FATAL("Accessing empty vector");
}
}
