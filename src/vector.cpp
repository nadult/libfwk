#include "fwk_base.h"
#include <stdio.h>

namespace fwk {

BaseVector::BaseVector(size_t tsize, int size, int capacity) : size(size), capacity(capacity) {
	auto nbytes = size_t(capacity) * tsize;
	data = (char *)malloc(nbytes);
	if(nbytes && !data)
		FATAL("Error while allocating memory: %d * %d bytes", capacity, (int)tsize);
}

BaseVector::BaseVector(BaseVector &&rhs) : data(rhs.data), size(rhs.size), capacity(rhs.capacity) {
	rhs.data = nullptr;
	rhs.size = rhs.capacity = 0;
}

BaseVector::BaseVector(size_t tsize, BinaryFunc copy_func, const BaseVector &rhs)
	: BaseVector(tsize, rhs.size, rhs.capacity) {
	for(int n = 0; n < size; n++)
		copy_func(data + tsize * n, rhs.data + tsize * n);
}

BaseVector::BaseVector(size_t tsize, BinaryFunc copy_func, char *ptr, int size)
	: BaseVector(tsize, size, size) {
	for(int n = 0; n < size; n++)
		copy_func(data + tsize * n, ptr + tsize * n);
}

void BaseVector::swap(BaseVector &rhs) {
	fwk::swap(data, rhs.data);
	fwk::swap(size, rhs.size);
	fwk::swap(capacity, rhs.capacity);
}

void BaseVector::free(size_t tsize, UnaryFunc destroy_func) {
	for(int n = 0; n < size; n++)
		destroy_func(data + tsize * n);
	::free(data);
	data = nullptr;
	size = capacity = 0;
}

int BaseVector::newCapacity(int min) const {
	return min > capacity * 2 ? min : (capacity == 0 ? initial_size : capacity * 2);
}

void BaseVector::reallocate(size_t tsize, BinaryFunc move_destroy_func, int new_capacity) {
	if(new_capacity <= capacity)
		return;

	BaseVector new_base(tsize, size, new_capacity);
	for(int n = 0; n < size; n++) {
		auto offset = tsize * n;
		move_destroy_func(new_base.data + offset, data + offset);
	}
	swap(new_base);
}

void BaseVector::resize(size_t tsize, UnaryFunc destroy_func, BinaryFunc move_destroy_func,
						BinaryFunc copy_func, void *obj, int new_size) {
	DASSERT(new_size >= 0);
	if(new_size > capacity)
		reallocate(tsize, move_destroy_func, newCapacity(new_size));

	while(size > new_size)
		destroy_func(data + tsize * --size);
	while(size < new_size)
		copy_func(data + tsize * size++, obj);
}

void BaseVector::insert(size_t tsize, BinaryFunc move_destroy_func, int index, int count) {
	DASSERT(index >= 0 && index <= size);
	int new_size = size + count;
	if(new_size > capacity)
		reallocate(tsize, move_destroy_func, newCapacity(new_size));

	int move_count = size - index;
	for(int n = move_count - 1; n >= 0; n--)
		move_destroy_func(data + tsize * (index + n + count), data + tsize * (index + n));
	size = new_size;
}

void BaseVector::clear(size_t tsize, UnaryFunc destroy_func) {
	for(int n = 0; n < size; n++)
		destroy_func(data + tsize * n);
	size = 0;
}

void BaseVector::erase(size_t tsize, UnaryFunc destroy_func, BinaryFunc move_destroy_func,
					   int index, int count) {
	DASSERT(index >= 0 && count >= 0 && index + count <= size);
	int move_start = index + count;
	int move_count = size - move_start;

	for(int n = 0; n < count; n++)
		destroy_func(data + tsize * (index + n));
	for(int n = 0; n < move_count; n++)
		move_destroy_func(data + tsize * (index + n), data + tsize * (index + count + n));
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
