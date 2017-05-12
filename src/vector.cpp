#include "fwk_base.h"
#ifdef FWK_TARGET_LINUX
#include <malloc.h>
static size_t usableSize(void *ptr, size_t size) { return malloc_usable_size(ptr); }
#else
static size_t usableSize(void *, size_t size) { return size; }
#endif
#include <stdio.h>

// TODO: there is still space for improvement (perf-wise) here
// TODO: more aggressive inlining here improves perf
namespace fwk {

#ifndef FWK_STD_VECTOR
void BaseVector::alloc(int obj_size, int size_, int capacity_) noexcept {
	size = size_;
	capacity = capacity_;
	auto nbytes = size_t(capacity) * obj_size;
	data = (char *)malloc(nbytes);
	if(nbytes && !data)
		FATAL("Error while allocating memory: %d * %d bytes", capacity, obj_size);
	capacity = usableSize(data, nbytes) / obj_size;
}

void BaseVector::swap(BaseVector &rhs) noexcept {
	fwk::swap(data, rhs.data);
	fwk::swap(size, rhs.size);
	fwk::swap(capacity, rhs.capacity);
}

int BaseVector::growCapacity(int obj_size) const noexcept {
	if(capacity == 0)
		return obj_size > 64 ? 1 : 64 / obj_size;
	if(capacity > 4096 * 32 / obj_size)
		return capacity * 2;
	return (capacity * 3 + 1) / 2;
}

int BaseVector::insertCapacity(int obj_size, int min_size) const noexcept {
	int cap = growCapacity(obj_size);
	return cap > min_size ? cap : min_size;
}

void BaseVector::copyConstruct(int obj_size, CopyFunc copy_func, char *ptr, int size) {
	alloc(obj_size, size, size);
	copy_func(data, ptr, size);
}

void BaseVector::reallocate(int obj_size, MoveDestroyFunc move_destroy_func, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector new_base;
	new_base.alloc(obj_size, size, new_capacity);
	move_destroy_func(new_base.data, data, size);
	swap(new_base);
}

void BaseVector::grow(int obj_size, MoveDestroyFunc move_destroy_func) {
	reallocate(obj_size, move_destroy_func, growCapacity(obj_size));
}

void BaseVector::resizePartial(int obj_size, DestroyFunc destroy_func,
							   MoveDestroyFunc move_destroy_func, int new_size) {
	DASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, insertCapacity(obj_size, new_size));

	if(size > new_size)
		destroy_func(data + size_t(obj_size) * new_size, size - new_size);
	size = new_size;
}

void BaseVector::assignPartial(int obj_size, DestroyFunc destroy_func, int new_size) noexcept {
	clear(destroy_func);
	if(new_size > capacity) {
		BaseVector new_base;
		new_base.alloc(obj_size, new_size, insertCapacity(obj_size, new_size));
		swap(new_base);
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
		reallocate(obj_size, move_destroy_func, insertCapacity(obj_size, new_size));

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

void BaseVector::clear(DestroyFunc destroy_func) noexcept {
	destroy_func(data, size);
	size = 0;
}

void BaseVector::erase(int obj_size, DestroyFunc destroy_func, MoveDestroyFunc move_destroy_func,
					   int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;

	destroy_func(data + size_t(obj_size) * index, count);
	move_destroy_func(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
					  move_count);
	size -= count;
}

void BaseVector::copyConstructPod(int obj_size, char *ptr, int size) noexcept {
	alloc(obj_size, size, size);
	memcpy(data, ptr, size_t(obj_size) * size);
}

void BaseVector::reallocatePod(int obj_size, int new_capacity) noexcept {
	if(new_capacity <= capacity)
		return;

	if(0 && data && size_t(capacity - size) * 16 < size_t(capacity)) {
		data = (char *)realloc(data, size_t(obj_size) * new_capacity);
		if(!data)
			FATAL("Error while re-allocating memory: %d * %d bytes", new_capacity, obj_size);
		capacity = usableSize(data, size_t(obj_size) * new_capacity) / obj_size;
		return;
	}

	BaseVector new_base;
	new_base.alloc(obj_size, size, new_capacity);
	memcpy(new_base.data, data, size_t(obj_size) * size);
	swap(new_base);
}

void BaseVector::growPod(int obj_size) noexcept {
	int current = capacity;
	reallocatePod(obj_size, growCapacity(obj_size));
}

void BaseVector::resizePodPartial(int obj_size, int new_size) noexcept {
	DASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocatePod(obj_size, insertCapacity(obj_size, new_size));
	size = new_size;
}

void BaseVector::assignPartialPod(int obj_size, int new_size) noexcept {
	clearPod();
	if(new_size > capacity) {
		BaseVector new_base;
		new_base.alloc(obj_size, new_size, insertCapacity(obj_size, new_size));
		swap(new_base);
		return;
	}
	size = new_size;
}

void BaseVector::assignPod(int obj_size, const void *ptr, int new_size) {
	assignPartialPod(obj_size, new_size);
	memcpy(data, ptr, size_t(obj_size) * size);
}

void BaseVector::insertPodPartial(int obj_size, int index, int count) noexcept {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocatePod(obj_size, insertCapacity(obj_size, new_size));

	int move_count = size - index;
	if(move_count > 0)
		memmove(data + size_t(obj_size) * (index + count), data + size_t(obj_size) * index,
				size_t(obj_size) * move_count);
	size = new_size;
}

void BaseVector::insertPod(int obj_size, int index, const void *ptr, int count) noexcept {
	insertPodPartial(obj_size, index, count);
	memcpy(data + size_t(obj_size) * index, ptr, size_t(obj_size) * count);
}

void BaseVector::erasePod(int obj_size, int index, int count) noexcept {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;
	if(move_count > 0)
		memmove(data + size_t(obj_size) * index, data + size_t(obj_size) * (index + count),
				size_t(obj_size) * move_count);
	size -= count;
}

#endif
}