#include "fwk_base.h"
#include <stdio.h>

namespace fwk {

void BaseVector::alloc(int obj_size, int size_, int capacity_) noexcept {
	size = size_;
	capacity = capacity_;
	auto nbytes = size_t(capacity) * obj_size;
	data = (char *)malloc(nbytes);
	if(nbytes && !data)
		FATAL("Error while allocating memory: %d * %d bytes", capacity, obj_size);
}

void BaseVector::swap(BaseVector &rhs) noexcept {
	fwk::swap(data, rhs.data);
	fwk::swap(size, rhs.size);
	fwk::swap(capacity, rhs.capacity);
}

int BaseVector::newCapacity(int min) const noexcept {
	return min > capacity * 2 ? min : (capacity == 0 ? initial_size : capacity * 2);
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
	int current = capacity;
	reallocate(obj_size, move_destroy_func, current == 0 ? BaseVector::initial_size : current * 2);
}

void BaseVector::resize(int obj_size, DestroyFunc destroy_func, MoveDestroyFunc move_destroy_func,
						InitFunc init_func, void *obj, int new_size) {
	DASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, newCapacity(new_size));

	if(size > new_size) {
		destroy_func(data, size - new_size);
		size = new_size;
	}
	if(size < new_size) {
		init_func(data + obj_size * size, obj, new_size - size);
		size = new_size;
	}
}

void BaseVector::assignPartial(int obj_size, DestroyFunc destroy_func, int new_size) noexcept {
	clear(destroy_func);
	if(new_size > capacity) {
		BaseVector new_base;
		new_base.alloc(obj_size, new_size, newCapacity(new_size));
		swap(new_base);
		return;
	}
	size = new_size;
}

void BaseVector::insert(int obj_size, MoveDestroyFunc move_destroy_func, int index, int count) {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocate(obj_size, move_destroy_func, newCapacity(new_size));

	int move_count = size - index;
	if(move_count > 0)
		move_destroy_func(data + obj_size * (index + count), data + obj_size * index, move_count);
	size = new_size;
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

	destroy_func(data + obj_size * index, count);
	move_destroy_func(data + obj_size * index, data + obj_size * (index + count), move_count);
	size -= count;
}

void BaseVector::copyConstructPod(int obj_size, char *ptr, int size) noexcept {
	alloc(obj_size, size, size);
	memcpy(data, ptr, size_t(obj_size) * size);
}

void BaseVector::reallocatePod(int obj_size, int new_capacity) noexcept {
	if(new_capacity <= capacity)
		return;

	BaseVector new_base;
	new_base.alloc(obj_size, size, new_capacity);
	memcpy(new_base.data, data, size_t(obj_size) * size);
	swap(new_base);
}

void BaseVector::growPod(int obj_size) noexcept {
	int current = capacity;
	reallocatePod(obj_size, current == 0 ? BaseVector::initial_size : current * 2);
}

void BaseVector::resizePod(int obj_size, InitFunc init_func, void *obj, int new_size) noexcept {
	DASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocatePod(obj_size, newCapacity(new_size));

	if(size < new_size)
		init_func(data + obj_size * size, obj, new_size - size);
	size = new_size;
}

void BaseVector::assignPartialPod(int obj_size, int new_size) noexcept {
	clearPod();
	if(new_size > capacity) {
		BaseVector new_base;
		new_base.alloc(obj_size, new_size, newCapacity(new_size));
		swap(new_base);
		return;
	}
	size = new_size;
}

void BaseVector::insertPod(int obj_size, int index, int count) noexcept {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocatePod(obj_size, newCapacity(new_size));

	int move_count = size - index;
	if(move_count > 0)
		memmove(data + obj_size * (index + count), data + obj_size * index,
				size_t(obj_size) * move_count);
	size = new_size;
}

void BaseVector::erasePod(int obj_size, int index, int count) noexcept {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;
	if(move_count > 0)
		memcpy(data + obj_size * index, data + obj_size * (index + count),
			   size_t(obj_size) * move_count);
	size -= count;
}

#ifdef TEST_VECTOR

void fatalError(const char *file, int line, const char *fmt, ...) {
	char new_fmt[1024];
	snprintf(new_fmt, sizeof(new_fmt), "%s:%d: %s", file, line, fmt);

	char buffer[4096];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), new_fmt, ap);
	va_end(ap);

#ifdef FWK_TARGET_HTML5
	printf("%s\n", buffer);
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", buffer);
	emscripten_force_exit(1);
#else
	printf("%s", buffer);
	exit(1);
#endif
}

void assertFailed(const char *file, int line, const char *text) {
	printf("assert failed: %s:%d : %s\n", file, line, text);
	exit(1);
}
#endif
}

#ifdef TEST_VECTOR
using namespace fwk;
void print(const vector<vector<int>> &vecs) {
	for(auto &v : vecs) {
		for(auto vv : v)
			printf("%d ", vv);
		printf("\n");
	}
}
int main() {
	Vector<int> vec = {10, 20, 40, 50};
	printf("size: %d\n", vec.size());

	vector<vector<int>> vvals;
	for(int k = 0; k < 4; k++)
		vvals.emplace_back(vec);
	vvals.erase(vvals.begin() + 1);
	print(vvals);

	/*
	vec.insert(vec.begin() + 2, 30);
	for(int n = 0; n < vec.size(); n++)
		printf("%d\n", vec[n]);
	printf("erase:\n");
	vec.erase(vec.begin() + 3, vec.end());
	for(int n = 0; n < vec.size(); n++)
		printf("%d\n", vec[n]);

	auto copy = vec;
	for(auto v : copy)
		printf("%d ", v);
	printf("\n"); */
	return 0;
}
#endif
