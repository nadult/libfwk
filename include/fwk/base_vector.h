// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/span.h"
#include "fwk/sys/memory.h"
#include "fwk/sys_base.h"

namespace fwk {

namespace detail {
	// Pool allocator for vectors
	constexpr int vpool_chunk_size = 128, vpool_num_chunks = 32;
	extern __thread char t_vpool_buf[vpool_chunk_size * vpool_num_chunks];
	extern __thread uint t_vpool_bits;

	static bool onVPool(const char *data) {
		return data >= t_vpool_buf && data < t_vpool_buf + vpool_chunk_size * vpool_num_chunks;
	}

}

int vectorGrowCapacity(int current, int obj_size);
int vectorInsertCapacity(int current, int obj_size, int min_size);
template <class T> int vectorInsertCapacity(int current, int min_size) {
	return vectorInsertCapacity(current, (int)sizeof(T), min_size);
}

// This class keeps implementation of vectors which is shared among different instantiations
// Most functions are in .cpp file to reduce code bloat (in most cases performance loss is negligible)
//
// TODO: VectorInfo struct keeping obj_size and pointers to functions?
// TODO: better naming of functions, add some description?
class BaseVector {
  public:
	using MoveDestroyFunc = void (*)(void *, void *, int);
	using DestroyFunc = void (*)(void *, int);
	using CopyFunc = void (*)(void *, const void *, int);

	~BaseVector() {} // Manual free is required

	void zero() {
		data = nullptr;
		size = capacity = 0;
	}
	void moveConstruct(BaseVector &&rhs) {
		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;
		rhs.zero();
	}

	void initializePool(int obj_size) {
		if(detail::t_vpool_bits) {
			using namespace detail;
			capacity = vpool_chunk_size / obj_size;
			int bit = countTrailingZeros(t_vpool_bits);
			t_vpool_bits &= ~(1 << bit);
			data = t_vpool_buf + bit * vpool_chunk_size;
			size = 0;
		} else {
			zero();
		}
	}

	void alloc(int obj_size, int size, int capacity);

	void free(int obj_size) {
		using namespace detail;
		if(onVPool(data))
			t_vpool_bits |= 1 << ((data - t_vpool_buf) / vpool_chunk_size);
		else
			fwk::deallocate(data);
	}

	void swap(BaseVector &rhs) {
		fwk::swap(data, rhs.data);
		fwk::swap(size, rhs.size);
		fwk::swap(capacity, rhs.capacity);
	}

	void grow(int obj_size, MoveDestroyFunc func);
	void growPod(int obj_size);

	void reallocate(int, MoveDestroyFunc move_destroy_func, int new_capacity);
	void clear(DestroyFunc destroy_func);
	void erase(int, DestroyFunc, MoveDestroyFunc, int index, int count);
	void resizePartial(int, DestroyFunc, MoveDestroyFunc, int new_size);
	void insertPartial(int, MoveDestroyFunc, int offset, int count);
	void insert(int, MoveDestroyFunc, CopyFunc, int offset, const void *, int count);
	void assignPartial(int, DestroyFunc, int new_size);
	void assign(int, DestroyFunc, CopyFunc, const void *, int size);

	void reallocatePod(int, int new_capacity);

	void reservePod(int, int desired_capacity);
	void reserve(int, MoveDestroyFunc, int desired_capacity);

	void clearPod() { size = 0; }
	void erasePod(int, int index, int count);
	void resizePodPartial(int, int new_size);
	void insertPodPartial(int, int offset, int count);
	void insertPod(int, int offset, const void *, int count);
	void assignPartialPod(int, int new_size);
	void assignPod(int, const void *, int size);

	void checkIndex(int index) const {
		if(index < 0 || index >= size)
			invalidIndex(index);
	}
	void checkNotEmpty() const {
		if(size == 0)
			invalidEmpty();
	}

	[[noreturn]] void invalidIndex(int index) const;
	[[noreturn]] void invalidEmpty() const;

	int size, capacity;
	char *data;
};

}
