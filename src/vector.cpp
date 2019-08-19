// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/base_vector_impl.h"
#include "fwk/sys/memory.h"
#include "fwk/vector.h"

namespace fwk {

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

}

template class fwk::BaseVector<false>;
