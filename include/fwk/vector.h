// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"
#include "fwk/sys_base.h"

namespace fwk {

class BaseVector {
  public:
	using MoveDestroyFunc = void (*)(void *, void *, int);
	using DestroyFunc = void (*)(void *, int);
	using CopyFunc = void (*)(void *, const void *, int);

	~BaseVector() { free(); }
	void free();
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

	static int growCapacity(int current, int obj_size);
	static int insertCapacity(int current, int obj_size, int min_size);

	template <class T> static int insertCapacity(int current, int min_size) {
		return insertCapacity(current, (int)sizeof(T), min_size);
	}

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

template <class T> class Vector {
  public:
	using value_type = T;
	using reference = value_type &;
	using const_reference = const value_type &;
	using iterator = T *;
	using const_iterator = const T *;
	using size_type = int;
	using difference_type = int;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	template <class IT>
	static constexpr bool is_input_iter = is_forward_iter<IT> &&is_constructible<T, IterBase<IT>>;

	Vector() { m_base.zero(); }
	~Vector() { destroy(m_base.data, m_base.size); }
	Vector(const Vector &rhs) {
		m_base.zero();
		assign(rhs.begin(), rhs.end());
	}
	Vector(Vector &&rhs) { m_base.moveConstruct(std::move(rhs.m_base)); }

	explicit Vector(int size, const T &default_value = T()) {
		m_base.alloc(sizeof(T), size, size);
		for(int idx = 0; idx < size; idx++)
			new(((T *)m_base.data) + idx) T(default_value);
	}
	template <class IT, EnableIf<is_input_iter<IT>>...> Vector(IT first, IT last) {
		m_base.zero();
		assign(first, last);
	}

	Vector(std::initializer_list<T> il) : Vector(il.begin(), il.end()) {}
	Vector(CSpan<T> span) : Vector(span.begin(), span.end()) {}
	template <class U, int size, EnableIf<is_same<RemoveConst<U>, T>>...>
	Vector(Span<U, size> span) : Vector(CSpan<T>(span)) {}

	void swap(Vector &rhs) { m_base.swap(rhs.m_base); }

	template <class IT, EnableIf<is_input_iter<IT>>...> void assign(IT first, IT last) {
		if(trivial_destruction)
			m_base.assignPartialPod(sizeof(T), fwk::distance(first, last));
		else
			m_base.assignPartial(sizeof(T), &Vector::destroy, fwk::distance(first, last));
		int offset = 0;
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
	}

	void assign(const_iterator first, const_iterator last) {
		if(trivial_copy_constr && trivial_destruction)
			m_base.assignPod(sizeof(T), first, last - first);
		else
			m_base.assign(sizeof(T), &Vector::destroy, &Vector::copy, first, last - first);
	}
	void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }
	void assign(int size, const T &value) {
		clear();
		resize(size, value);
	}

	const Vector &operator=(const Vector &rhs) {
		if(this == &rhs)
			return *this;
		assign(rhs.begin(), rhs.end());
		return *this;
	}

	const Vector &operator=(Vector &&rhs) {
		if(this == &rhs)
			return *this;
		m_base.swap(rhs.m_base);
		rhs.clear();
		return *this;
	}

	bool inRange(int idx) const { return fwk::inRange(idx, 0, m_base.size); }
	const T *data() const { return reinterpret_cast<const T *>(m_base.data); }
	T *data() { return reinterpret_cast<T *>(m_base.data); }

	T &operator[](int idx) {
		IF_PARANOID(m_base.checkIndex(idx));
		return data()[idx];
	}
	const T &operator[](int idx) const {
		IF_PARANOID(m_base.checkIndex(idx));
		return data()[idx];
	}

	const T &front() const {
		IF_PARANOID(m_base.checkNotEmpty());
		return data()[0];
	}
	T &front() {
		IF_PARANOID(m_base.checkNotEmpty());
		return data()[0];
	}
	const T &back() const {
		IF_PARANOID(m_base.checkNotEmpty());
		return data()[m_base.size - 1];
	}
	T &back() {
		IF_PARANOID(m_base.checkNotEmpty());
		return data()[m_base.size - 1];
	}

	int size() const { return m_base.size; }
	int capacity() const { return m_base.capacity; }
	i64 usedMemory() const { return m_base.size * (i64)sizeof(T); }

	bool empty() const { return m_base.size == 0; }
	explicit operator bool() const { return m_base.size > 0; }

	void clear() { m_base.clear(&Vector::destroy); }
	void free() {
		Vector empty;
		m_base.swap(empty.m_base);
	}

	void reserve(int new_capacity) {
		if(trivial_move_constr && trivial_destruction)
			m_base.reservePod(sizeof(T), new_capacity);
		else
			m_base.reserve(sizeof(T), &Vector::moveAndDestroy, new_capacity);
	}

	void resize(int new_size, T default_value) {
		int index = m_base.size;
		if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
			m_base.resizePodPartial(sizeof(T), new_size);
		else
			m_base.resizePartial(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, new_size);
		while(index < new_size)
			new(data() + index++) T(default_value);
	}
	void resize(int new_size) {
		int index = m_base.size;
		if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
			m_base.resizePodPartial(sizeof(T), new_size);
		else
			m_base.resizePartial(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, new_size);
		while(index < new_size)
			new(data() + index++) T();
	}
	void shrink(int new_size) {
		PASSERT(m_base.size >= new_size);
		if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
			m_base.resizePodPartial(sizeof(T), new_size);
		else
			m_base.resizePartial(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, new_size);
	}

	template <class... Args> T &emplace_back(Args &&... args) INST_EXCEPT {
		if(m_base.size == m_base.capacity) {
			if(trivial_move_constr && trivial_destruction)
				m_base.growPod(sizeof(T));
			else
				m_base.grow(sizeof(T), &Vector::moveAndDestroy);
		}
		new(end()) T{std::forward<Args>(args)...};
		T &back = (reinterpret_cast<T *>(m_base.data))[m_base.size];
		m_base.size++;
		return back;
	}
	void push_back(const T &rhs) { emplace_back(rhs); }
	void push_back(T &&rhs) { emplace_back(std::move(rhs)); }

	void erase(const_iterator it) {
		if(trivial_move_constr && trivial_destruction)
			m_base.erasePod(sizeof(T), it - begin(), 1);
		else
			m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, it - begin(), 1);
	}
	void pop_back() {
		IF_PARANOID(m_base.checkNotEmpty());
		back().~T();
		m_base.size--;
	}

	void erase(const_iterator a, const_iterator b) {
		if(trivial_move_constr && trivial_destruction)
			m_base.erasePod(sizeof(T), a - begin(), b - a);
		else
			m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, a - begin(), b - a);
	}

	iterator insert(const const_iterator pos, const T &value) {
		return insert(pos, &value, (&value) + 1);
	}

	template <class IT, EnableIf<is_input_iter<IT>>...>
	iterator insert(const const_iterator pos, IT first, IT last) {
		int offset = pos - begin();
		if(trivial_move_constr && trivial_destruction)
			m_base.insertPodPartial(sizeof(T), offset, fwk::distance(first, last));
		else
			m_base.insertPartial(sizeof(T), &Vector::moveAndDestroyBackwards, offset,
								 fwk::distance(first, last));
		int toffset = offset;
		while(!(first == last)) {
			new(data() + offset++) T(*first);
			++first;
		}
		return begin() + toffset;
	}

	iterator insert(const const_iterator pos, const_iterator first, const_iterator last) {
		if(pos == end() && last - first <= m_base.capacity - m_base.size) {
			if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
				memcpy(end(), first, (last - first) * sizeof(T));
			else
				copy(end(), first, last - first);
			auto out = end();
			m_base.size += last - first;
			return out;
		}

		int offset = pos - begin();
		if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
			m_base.insertPod(sizeof(T), offset, first, last - first);
		else
			m_base.insert(sizeof(T), &Vector::moveAndDestroyBackwards, &Vector::copy, offset, first,
						  last - first);
		return begin() + offset;
	}

	iterator insert(const_iterator pos, std::initializer_list<T> il) {
		int offset = pos - begin();
		insert(pos, il.begin(), il.end());
		return begin() + offset;
	}

	T *begin() { return data(); }
	T *end() { return data() + m_base.size; }
	const T *begin() const { return data(); }
	const T *end() const { return data() + m_base.size; }

	operator Span<T>() { return Span<T>(data(), m_base.size); }
	operator CSpan<T>() const { return CSpan<T>(data(), m_base.size); }

	bool operator==(const Vector &rhs) const {
		return size() == rhs.size() && std::equal(begin(), end(), rhs.begin(), rhs.end());
	}

	bool operator<(const Vector &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

	template <class U> Vector<U> reinterpret() {
		static_assert(compatibleSizes(sizeof(T), sizeof(U)),
					  "Incompatible sizes; are you sure, you want to do this cast?");

		m_base.size = (int)(size_t(m_base.size) * sizeof(T) / sizeof(U));
		auto *rcurrent = reinterpret_cast<Vector<U> *>(this);
		return Vector<U>(move(*rcurrent));
	}

  private:
	static constexpr bool trivial_move_constr = std::is_trivially_move_constructible<T>::value,
						  trivial_copy_constr = std::is_trivially_copy_constructible<T>::value,
						  trivial_destruction = std::is_trivially_destructible<T>::value;

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
			new(dst + n) T(std::move(src[n]));
			src[n].~T();
		}
	}
	static void moveAndDestroyBackwards(void *vdst, void *vsrc, int count) {
		T *__restrict__ src = (T *)vsrc;
		T *__restrict__ dst = (T *)vdst;
		for(int n = count - 1; n >= 0; n--) {
			new(dst + n) T(std::move(src[n]));
			src[n].~T();
		}
	}
	static void destroy(void *vsrc, int count) {
		T *src = (T *)vsrc;
		for(int n = 0; n < count; n++)
			src[n].~T();
	}

	BaseVector m_base;
	friend class PodVector<T>;
};
}
