// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/base_vector.h"
#include "fwk/sys/memory.h"

// TODO: there is still space for improvement (perf-wise) here
// TODO: more aggressive inlining here improves perf
namespace fwk {

namespace detail {
	FWK_THREAD_LOCAL char t_vpool_buf[vpool_chunk_size * vpool_num_chunks];
	FWK_THREAD_LOCAL u32 t_vpool_bits = 0xffffffff;
}

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

void BaseVector::alloc(int obj_size, int size_, int capacity_) {
	size = size_;
	capacity = capacity_;
	data = (char *)fwk::allocate(size_t(capacity) * obj_size);
}

void BaseVector::grow(int obj_size, MoveDestroyFunc func) {
	reallocate(obj_size, func, vectorGrowCapacity(capacity, obj_size));
}
void BaseVector::growPod(int obj_size) {
	reallocatePod(obj_size, vectorGrowCapacity(capacity, obj_size));
}

void BaseVector::reallocate(int obj_size, MoveDestroyFunc move_destroy_func, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector temp;
	temp.alloc(obj_size, size, new_capacity);
	move_destroy_func(temp.data, data, size);
	swap(temp);
	temp.free(obj_size);
}

void BaseVector::resizePartial(int obj_size, DestroyFunc destroy_func,
							   MoveDestroyFunc move_destroy_func, int new_size) {
	PASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, vectorInsertCapacity(capacity, obj_size, new_size));

	if(size > new_size)
		destroy_func(data + size_t(obj_size) * new_size, size - new_size);
	size = new_size;
}

void BaseVector::assignPartial(int obj_size, DestroyFunc destroy_func, int new_size) {
	clear(destroy_func);
	if(new_size > capacity) {
		BaseVector temp;
		temp.alloc(obj_size, new_size, vectorInsertCapacity(capacity, obj_size, new_size));
		swap(temp);
		temp.free(obj_size);
		return;
	}
	size = new_size;
}

void BaseVector::assign(int obj_size, DestroyFunc destroy_func, CopyFunc copy_func, const void *ptr,
						int new_size) {
	assignPartial(obj_size, destroy_func, new_size);
	copy_func(data, ptr, size);
}

void BaseVector::insertPartial(int obj_size, MoveDestroyFunc move_destroy_func, int index,
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

void BaseVector::insert(int obj_size, MoveDestroyFunc move_destroy_func, CopyFunc copy_func,
						int index, const void *ptr, int count) {
	insertPartial(obj_size, move_destroy_func, index, count);
	copy_func(data + size_t(obj_size) * index, ptr, count);
}

void BaseVector::clear(DestroyFunc destroy_func) {
	destroy_func(data, size);
	size = 0;
}

void BaseVector::erase(int obj_size, DestroyFunc destroy_func, MoveDestroyFunc move_destroy_func,
					   int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	if(!count)
		return;

	int move_start = index + count;
	int move_count = size - move_start;

	destroy_func(data + size_t(obj_size) * index, count);
	move_destroy_func(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
					  move_count);
	size -= count;
}

void BaseVector::reallocatePod(int obj_size, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector temp;
	temp.alloc(obj_size, size, new_capacity);
	memcpy(temp.data, data, size_t(obj_size) * size);
	swap(temp);
	temp.free(obj_size);
}

void BaseVector::reservePod(int obj_size, int desired_capacity) {
	if(desired_capacity > capacity) {
		int new_capacity = vectorInsertCapacity(capacity, obj_size, desired_capacity);
		reallocatePod(obj_size, new_capacity);
	}
}

void BaseVector::reserve(int obj_size, MoveDestroyFunc func, int desired_capacity) {
	if(desired_capacity > capacity) {
		int new_capacity = vectorInsertCapacity(capacity, obj_size, desired_capacity);
		reallocate(obj_size, func, new_capacity);
	}
}

void BaseVector::resizePodPartial(int obj_size, int new_size) {
	PASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocatePod(obj_size, vectorInsertCapacity(capacity, obj_size, new_size));
	size = new_size;
}

void BaseVector::assignPartialPod(int obj_size, int new_size) {
	clearPod();
	if(new_size > capacity) {
		BaseVector temp;
		temp.alloc(obj_size, new_size, vectorInsertCapacity(capacity, obj_size, new_size));
		swap(temp);
		temp.free(obj_size);
		return;
	}
	size = new_size;
}

void BaseVector::assignPod(int obj_size, const void *ptr, int new_size) {
	assignPartialPod(obj_size, new_size);
	memcpy(data, ptr, size_t(obj_size) * size);
}

void BaseVector::insertPodPartial(int obj_size, int index, int count) {
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

void BaseVector::insertPod(int obj_size, int index, const void *ptr, int count) {
	insertPodPartial(obj_size, index, count);
	memcpy(data + size_t(obj_size) * index, ptr, size_t(obj_size) * count);
}

void BaseVector::erasePod(int obj_size, int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;
	if(move_count > 0)
		memmove(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
				size_t(obj_size) * move_count);
	size -= count;
}

void BaseVector::invalidIndex(int index) const {
	FWK_FATAL("Index %d out of range: <%d; %d)", index, 0, size);
}

void BaseVector::invalidEmpty() const { FWK_FATAL("Accessing empty vector"); }
}
