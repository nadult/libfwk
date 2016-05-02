#pragma once

#include "fwk_base.h"

namespace fwk {

class BaseVector {
  public:
	using MoveDestroyFunc = void (*)(void *, void *, int);
	using DestroyFunc = void (*)(void *, int);
	using InitFunc = void (*)(void *, const void *, int);
	using CopyFunc = void (*)(void *, const void *, int);

	~BaseVector() noexcept { ::free(data); }
	void zero() noexcept {
		data = nullptr;
		size = capacity = 0;
	}
	void moveConstruct(BaseVector &&rhs) noexcept {
		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;
		rhs.zero();
	}
	void alloc(int, int size, int capacity) noexcept;
	void swap(BaseVector &) noexcept;

	enum { initial_size = 8 };

	int newCapacity(int min) const noexcept;

	void copyConstruct(int, CopyFunc, char *, int size);
	void grow(int, MoveDestroyFunc);
	void reallocate(int, MoveDestroyFunc move_destroy_func, int new_capacity);
	void clear(DestroyFunc destroy_func) noexcept;
	void erase(int, DestroyFunc, MoveDestroyFunc, int index, int count);
	void resize(int, DestroyFunc, MoveDestroyFunc, InitFunc, void *obj, int new_size);
	void insert(int, MoveDestroyFunc, int offset, int count);
	void assignPartial(int, DestroyFunc, int new_size) noexcept;

	void copyConstructPod(int, char *, int size) noexcept;
	void growPod(int) noexcept;
	void reallocatePod(int, int new_capacity) noexcept;
	void clearPod() noexcept { size = 0; }
	void erasePod(int, int index, int count) noexcept;
	void resizePod(int, InitFunc, void *obj, int new_size) noexcept;
	void insertPod(int, int offset, int count) noexcept;
	void assignPartialPod(int, int new_size) noexcept;
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
	typedef int size_type;
	typedef typename std::make_signed<size_type>::type difference_type;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	Vector() { m_base.zero(); }
	~Vector() { destroy(m_base.data, m_base.size); }
	Vector(const Vector &rhs) {
		if(isPod())
			m_base.copyConstructPod(sizeof(T), rhs.m_base.data, rhs.m_base.size);
		else
			m_base.copyConstruct(sizeof(T), &Vector::copy, rhs.m_base.data, rhs.m_base.size);
	}
	Vector(Vector &&rhs) { m_base.moveConstruct(move(rhs.m_base)); }

	Vector(int size, const T &value = T()) {
		m_base.zero();
		resize(size, value);
	}
	template <class Iter, typename = std::_RequireInputIter<Iter>> Vector(Iter first, Iter last) {
		m_base.zero();
		assign(first, last);
	}

	Vector(std::initializer_list<T> il) : Vector(il.begin(), il.end()) {}

	void swap(Vector &rhs) { m_base.swap(rhs.m_base); }

	template <class Iter, typename = std::_RequireInputIter<Iter>>
	void assign(Iter first, Iter last) {
		if(isPod())
			m_base.assignPartialPod(sizeof(T), last - first);
		else
			m_base.assignPartial(sizeof(T), &Vector::destroy, last - first);
		int offset = 0;
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
	}

	constexpr bool isPod() const { return IsPod<T>::value; }

	void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }
	void assign(int size, const T &value) {
		clear();
		resize(size, value);
	}

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

	void clear() { m_base.clear(&Vector::destroy); }

	void reserve(int new_capacity) { reallocate(m_base.newCapacity(new_capacity)); }
	void resize(int new_size, T default_value = T()) {
		if(isPod())
			m_base.resizePod(sizeof(T), &Vector::init, &default_value, new_size);
		else
			m_base.resize(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, &Vector::init,
						  &default_value, new_size);
	}

	template <class... Args> void emplace_back(Args &&... args) {
		if(m_base.size == m_base.capacity) {
			if(isPod())
				m_base.growPod(sizeof(T));
			else
				m_base.grow(sizeof(T), &Vector::moveAndDestroy);
		}
		new(end()) T(std::forward<Args>(args)...);
		m_base.size++;
	}
	void push_back(const T &rhs) { emplace_back(rhs); }

	void erase(const_iterator it) {
		if(isPod())
			m_base.erasePod(sizeof(T), it - begin(), 1);
		else
			m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, it - begin(), 1);
	}
	void pop_back() {
		PASSERT(size() > 0);
		if(!isPod())
			back().~T();
		m_base.size--;
	}

	void erase(const_iterator a, const_iterator b) {
		if(isPod())
			m_base.erasePod(sizeof(T), a - begin(), b - a);
		else
			m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, a - begin(), b - a);
	}

	iterator insert(const const_iterator pos, const T &value) {
		return insert(pos, &value, (&value) + 1);
	}
	template <class Iter> iterator insert(const const_iterator pos, Iter first, Iter last) {
		int offset = pos - begin();
		if(isPod())
			m_base.insertPod(sizeof(T), offset, last - first);
		else
			m_base.insert(sizeof(T), &Vector::moveAndDestroyBackwards, offset, last - first);
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
		const T *__restrict__ src = (T *)vsrc;
		T *__restrict__ dst = (T *)vdst;
		for(int n = 0; n < count; n++)
			new(dst + n) T(src[n]);
	}
	static void moveAndDestroy(void *vdst, void *vsrc, int count) {
		T *__restrict__ src = (T *)vsrc;
		T *__restrict__ dst = (T *)vdst;
		for(int n = 0; n < count; n++) {
			new(dst + n) T(move(src[n]));
			src[n].~T();
		}
	}
	static void moveAndDestroyBackwards(void *vdst, void *vsrc, int count) {
		T *__restrict__ src = (T *)vsrc;
		T *__restrict__ dst = (T *)vdst;
		for(int n = count - 1; n >= 0; n--) {
			new(dst + n) T(move(src[n]));
			src[n].~T();
		}
	}
	static void init(void *vdst, const void *vsrc, int count) {
		const T *__restrict__ src = (T *)vsrc;
		T *__restrict__ dst = (T *)vdst;
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

	BaseVector m_base;
};

template <class T> using vector = Vector<T>;
}
