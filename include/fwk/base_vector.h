// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"
#include "fwk/sys_base.h"

namespace fwk {

// Implemented in vector.cpp
int vectorGrowCapacity(int current, int obj_size);
int vectorInsertCapacity(int current, int obj_size, int min_size);
template <class T> int vectorInsertCapacity(int current, int min_size) {
	return vectorInsertCapacity(current, (int)sizeof(T), min_size);
}

template <bool pool_alloc> class BaseVector {
  public:
	using MoveDestroyFunc = void (*)(void *, void *, int);
	using DestroyFunc = void (*)(void *, int);
	using CopyFunc = void (*)(void *, const void *, int);

	~BaseVector() {} // Manual free is required
	void free(int obj_size);

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
	void alloc(int, int size, int capacity);
	void swap(BaseVector &);

	// TODO: describe what these do
	void grow(int, MoveDestroyFunc);
	void reallocate(int, MoveDestroyFunc move_destroy_func, int new_capacity);
	void clear(DestroyFunc destroy_func);
	void erase(int, DestroyFunc, MoveDestroyFunc, int index, int count);
	void resizePartial(int, DestroyFunc, MoveDestroyFunc, int new_size);
	void insertPartial(int, MoveDestroyFunc, int offset, int count);
	void insert(int, MoveDestroyFunc, CopyFunc, int offset, const void *, int count);
	void assignPartial(int, DestroyFunc, int new_size);
	void assign(int, DestroyFunc, CopyFunc, const void *, int size);

	void growPod(int);
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
}
