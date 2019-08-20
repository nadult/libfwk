// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/base_vector.h"
#include "fwk/sys/memory.h"

// TODO: there is still space for improvement (perf-wise) here
// TODO: more aggressive inlining here improves perf
namespace fwk {

__thread char *detail::t_vpool_buf[vpool_max_size];
__thread int detail::t_vpool_size = 0;

int vectorGrowCapacity(int capacity, int obj_size) {
	if(capacity == 0)
		return obj_size > 64 ? 1 : 64 / obj_size;
	if(capacity > 4096 * 32 / obj_size)
		return capacity * 2;
	return (capacity * 3 + 1) / 2;
}

int vectorInsertCapacity(int capacity, int obj_size, int min_size) {
	int cap = vectorGrowCapacity(capacity, obj_size);
	return cap > min_size ? cap : min_size;
}

template <bool pa> void BaseVector<pa>::alloc(int obj_size, int size_, int capacity_) {
	size = size_;
	if constexpr(pa) {
		using namespace detail;
		auto nbytes = max(capacity_ * obj_size, vpool_alloc_size);
		capacity = nbytes / obj_size;
		if(nbytes > vpool_alloc_size || !t_vpool_size)
			data = (char *)fwk::allocate(nbytes);
		else
			data = t_vpool_buf[--t_vpool_size];
	} else {
		capacity = capacity_;
		data = (char *)fwk::allocate(size_t(capacity) * obj_size);
	}
}

template <bool pa> void BaseVector<pa>::grow(int obj_size, MoveDestroyFunc func) {
	reallocate(obj_size, func, vectorGrowCapacity(capacity, obj_size));
}
template <bool pa> void BaseVector<pa>::growPod(int obj_size) {
	reallocatePod(obj_size, vectorGrowCapacity(capacity, obj_size));
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

template class fwk::BaseVector<false>;
template class fwk::BaseVector<true>;
}
