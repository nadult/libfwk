// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"
#include "fwk/sys/memory.h"
#include "fwk/sys_base.h"

namespace fwk {

namespace detail {
	// Pool allocator for vectors
	constexpr int vpool_alloc_size = 128, vpool_max_size = 64;
	extern __thread char *t_vpool_buf[vpool_max_size];
	extern __thread int t_vpool_size;
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
// TODO: separate pools for different types ?
// TODO: better naming of functions, add some description?
template <bool pool_alloc> class BaseVector {
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

	void initialize(int obj_size) {
		if(pool_alloc && detail::t_vpool_size) {
			using namespace detail;
			capacity = vpool_alloc_size / obj_size;
			data = t_vpool_buf[--t_vpool_size];
			size = 0;
		} else {
			zero();
		}
	}

	void alloc(int obj_size, int size, int capacity);

	void free(int obj_size) {
		if constexpr(pool_alloc) {
			using namespace detail;
			auto nbytes = size_t(capacity) * obj_size;
			if(nbytes > vpool_alloc_size || t_vpool_size == vpool_max_size)
				fwk::deallocate(data);
			else if(data)
				t_vpool_buf[t_vpool_size++] = data;
		} else
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

extern template class BaseVector<false>;
extern template class BaseVector<true>;
}
