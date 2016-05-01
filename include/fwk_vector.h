#pragma once

#include "fwk_base.h"

namespace fwk {

class BaseVector {
  public:
	using BinaryFunc = void (*)(void *, void *);
	using UnaryFunc = void (*)(void *);

	BaseVector() : data(0), size(0), capacity(0) {}
	BaseVector(size_t tsize, int size, int capacity);
	BaseVector(size_t, BinaryFunc, const BaseVector &);
	BaseVector(size_t, BinaryFunc, char *, int size);
	BaseVector(BaseVector &&);
	void swap(BaseVector &);

	enum { initial_size = 8 };

	int newCapacity(int min) const;
	void reallocate(size_t tsize, BinaryFunc move_destroy_func, int new_capacity);
	void clear(size_t tsize, UnaryFunc destroy_func);
	void erase(size_t, UnaryFunc, BinaryFunc, int index, int count);
	void free(size_t, UnaryFunc);
	void resize(size_t, UnaryFunc, BinaryFunc, BinaryFunc, void *obj, int new_size);
	void insert(size_t, BinaryFunc, int offset, int count);

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
	~Vector() { m_base.free(sizeof(T), &Vector::destroyObject); }
	Vector(const Vector &rhs) : m_base(sizeof(T), &Vector::copyObject, rhs.m_base) {}
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

	void clear() { m_base.clear(sizeof(T), &Vector::destroyObject); }

	void reserve(int new_capacity) { reallocate(m_base.newCapacity(new_capacity)); }
	void resize(int new_size, T default_value = T()) {
		m_base.resize(sizeof(T), &Vector::destroyObject, &Vector::moveAndDestroyObject,
					  &Vector::copyObject, &default_value, new_size);
	}

	template <class... Args> void emplace_back(Args &&... args) {
		if(m_base.size == m_base.capacity)
			grow();
		int index = m_base.size++;
		new(m_base.data + sizeof(T) * index) T(std::forward<Args>(args)...);
	}
	void push_back(const T &rhs) { emplace_back(rhs); }

	void erase(const_iterator it) {
		m_base.erase(sizeof(T), &Vector::destroyObject, &Vector::moveAndDestroyObject, it - begin(),
					 1);
	}
	void pop_back() { erase(end() - 1); }

	void erase(const_iterator a, const_iterator b) {
		m_base.erase(sizeof(T), &Vector::destroyObject, &Vector::moveAndDestroyObject, a - begin(),
					 b - a);
	}

	iterator insert(const const_iterator pos, const T &value) {
		return insert(pos, &value, (&value) + 1);
	}
	template <class Iter> iterator insert(const const_iterator pos, Iter first, Iter last) {
		int offset = pos - begin();
		m_base.insert(sizeof(T), &Vector::moveAndDestroyObject, offset, last - first);
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
		return begin() + offset;
	}

	iterator insert(const_iterator pos, std::initializer_list<T> il) {
		insert(pos, il.begin(), il.end());
		return begin() + (pos - begin());
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
	static void copyObject(void *dst, void *src) { new(dst) T(*reinterpret_cast<const T *>(src)); }
	static void moveAndDestroyObject(void *dst, void *src) {
		auto &source = *reinterpret_cast<T *>(src);
		new(dst) T(move(source));
		source.~T();
	}
	static void moveObject(void *dst, void *src) {
		auto &source = *reinterpret_cast<T *>(src);
		new(dst) T(move(source));
	}
	static void destroyObject(void *src) {
		auto &source = *reinterpret_cast<T *>(src);
		source.~T();
	}

	void reallocate(int new_capacity) {
		m_base.reallocate(sizeof(T), &Vector::moveAndDestroyObject, new_capacity);
	}

	void grow() {
		int current = capacity();
		reallocate(current == 0 ? BaseVector::initial_size : current * 2);
	};

	BaseVector m_base;
};

template <class T> using vector = Vector<T>;
}
