#pragma once

#include "fwk_base.h"

namespace fwk {

class BaseVector {
  public:
	using MoveDestroyFunc = void (*)(void *, void *, int);
	using DestroyFunc = void (*)(void *, int);
	using InitFunc = void (*)(void *, const void *, int);
	using CopyFunc = void (*)(void *, const void *, int);

	BaseVector() : data(0), size(0), capacity(0) {}
	BaseVector(size_t tsize, int size, int capacity);
	BaseVector(size_t, CopyFunc, const BaseVector &);
	BaseVector(size_t, CopyFunc, char *, int size);
	BaseVector(BaseVector &&);
	void swap(BaseVector &);
	~BaseVector();

	enum { initial_size = 8 };

	int newCapacity(int min) const;
	void reallocate(size_t tsize, MoveDestroyFunc move_destroy_func, int new_capacity);
	void clear(size_t tsize, DestroyFunc destroy_func);
	void erase(size_t, DestroyFunc, MoveDestroyFunc, int index, int count);
	void resize(size_t, DestroyFunc, MoveDestroyFunc, InitFunc, void *obj, int new_size);
	void insert(size_t, MoveDestroyFunc, int offset, int count);

	char *data;
	int size, capacity;
};

template <class T> class Vector {
  public:
	typedef T value_type;
	typedef value_type &reference;
	typedef const value_type &const_reference;
	typedef T *iterator;
	typedef const T *const_iterator;
	typedef size_t size_type;
	typedef typename std::make_signed<size_type>::type difference_type;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	Vector() {}
	~Vector() { destroy(m_base.data, m_base.size); }
	Vector(const Vector &rhs) : m_base(sizeof(T), &Vector::copy, rhs.m_base) {}
	Vector(Vector &&rhs) : m_base(move(rhs.m_base)) {}
	Vector(int size, const T &value = T()) { resize(size, value); }
	template <class Iter, typename = std::_RequireInputIter<Iter>>
	Vector(Iter first, Iter last) : m_base(sizeof(T), last - first, last - first) {
		int offset = 0;
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
	}

	Vector(std::initializer_list<T> il) : Vector(il.begin(), il.end()) {}

	void swap(Vector &rhs) { m_base.swap(rhs.m_base); }

	void operator=(const Vector &rhs) {
		if(this == &rhs)
			return;
		// TODO: option without reallocation?
		Vector copy(rhs);
		swap(copy);
	}

	void operator=(Vector &&rhs) {
		if(this == &rhs)
			return;
		m_base.swap(rhs.m_base);
	}

	bool validIndex(int idx) { return idx >= 0 && idx < m_base.size; }
	const T *data() const { return reinterpret_cast<const T *>(m_base.data); }
	T *data() { return reinterpret_cast<T *>(m_base.data); }

	T &operator[](int idx) {
		PASSERT(validIndex(idx));
		return data()[idx];
	}
	const T &operator[](int idx) const {
		PASSERT(validIndex(idx));
		return data()[idx];
	}

	const T &front() const { return data()[0]; }
	T &front() { return data()[0]; }
	const T &back() const { return data()[m_base.size - 1]; }
	T &back() { return data()[m_base.size - 1]; }

	int size() const { return m_base.size; }
	int capacity() const { return m_base.capacity; }
	bool empty() const { return m_base.size == 0; }

	void clear() { m_base.clear(sizeof(T), &Vector::destroy); }

	void reserve(int new_capacity) { reallocate(m_base.newCapacity(new_capacity)); }
	void resize(int new_size, T default_value = T()) {
		m_base.resize(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, &Vector::init,
					  &default_value, new_size);
	}

	template <class... Args> void emplace_back(Args &&... args) {
		if(m_base.size == m_base.capacity)
			grow();
		new(end()) T(std::forward<Args>(args)...);
		m_base.size++;
	}
	void push_back(const T &rhs) { emplace_back(rhs); }

	void erase(const_iterator it) {
		m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, it - begin(), 1);
	}
	void pop_back() { erase(end() - 1); }

	void erase(const_iterator a, const_iterator b) {
		m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, a - begin(), b - a);
	}

	iterator insert(const const_iterator pos, const T &value) {
		return insert(pos, &value, (&value) + 1);
	}
	template <class Iter> iterator insert(const const_iterator pos, Iter first, Iter last) {
		int offset = pos - begin();
		m_base.insert(sizeof(T), &Vector::moveAndDestroy, offset, last - first);
		int toffset = offset;
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
		return begin() + toffset;
	}

	iterator insert(const_iterator pos, std::initializer_list<T> il) {
		int offset = pos - begin();
		insert(pos, il.begin(), il.end());
		return begin() + offset;
	}

	auto begin() { return data(); }
	auto end() { return data() + size(); }
	auto begin() const { return data(); }
	auto end() const { return data() + size(); }

	bool operator==(const Vector &rhs) const {
		if(m_base.size != rhs.m_base.size)
			return false;
		for(int n = 0; n < m_base.size; n++)
			if(!((*this)[n] == rhs[n]))
				return false;
		return true;
	}

	bool operator<(const Vector &rhs) const {
		int count = m_base.size < rhs.m_base.size ? m_base.size : rhs.m_base.size;
		for(int n = 0; n < count; n++)
			if((*this)[n] < rhs[n])
				return false;
		return m_base.size < rhs.m_base.size;
	}

  private:
	static void copy(void *vdst, const void *vsrc, int count) {
		const T *src = (T *)vsrc;
		T *dst = (T *)vdst;
		for(int n = 0; n < count; n++)
			new(dst + n) T(src[n]);
	}
	static void moveAndDestroy(void *vdst, void *vsrc, int count) {
		T *src = (T *)vsrc;
		T *dst = (T *)vdst;
		for(int n = 0; n < count; n++) {
			new(dst + n) T(move(src[n]));
			src[n].~T();
		}
	}
	static void init(void *vdst, const void *vsrc, int count) {
		const T *src = (T *)vsrc;
		T *dst = (T *)vdst;
		for(int n = 0; n < count; n++)
			new(dst + n) T(*src);
	}

	static void destroy(void *vsrc, int count) {
		T *src = (T *)vsrc;
		for(int n = 0; n < count; n++)
			src[n].~T();
	}

	void reallocate(int new_capacity) {
		m_base.reallocate(sizeof(T), &Vector::moveAndDestroy, new_capacity);
	}

	void grow() {
		int current = capacity();
		reallocate(current == 0 ? BaseVector::initial_size : current * 2);
	};

	BaseVector m_base;
};

template <class T> using vector = Vector<T>;
}
