// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/base_vector.h"
#include "fwk/span.h"
#include "fwk/sys_base.h"

namespace fwk {

struct PoolAllocTag {};
constexpr PoolAllocTag pool_alloc;

// Regular Vector class with several improvements:
// - fast compilation & minimized code size
// - Pooled allocation support (4KB per thread); Pool-allocated vectors should be destroyed
//   on the same thread on which they were created, otherwise pools will be wasted and give no
//   performance gain
template <class T> class Vector {
  public:
	using value_type = T;
	using size_type = int;

	template <class IT>
	static constexpr bool is_input_iter = is_forward_iter<IT> &&is_constructible<T, IterBase<IT>>;

	explicit Vector(PoolAllocTag) { m_base.initializePool(sizeof(T)); }
	Vector() { m_base.zero(); }

	~Vector() {
		destroy(m_base.data, m_base.size);
		m_base.free(sizeof(T));
	}
	Vector(const Vector &rhs) : Vector() { assign(rhs.begin(), rhs.end()); }
	Vector(Vector &&rhs) { m_base.moveConstruct(std::move(rhs.m_base)); }

	explicit Vector(PoolAllocTag, int size, const T &default_value = T()) {
		if(detail::t_vpool_bits && size <= int(detail::vpool_chunk_size / sizeof(T))) {
			m_base.initializePool(sizeof(T));
			m_base.size = size;
		} else
			m_base.alloc(sizeof(T), size, size);

		for(int idx = 0; idx < size; idx++)
			new(((T *)m_base.data) + idx) T(default_value);
	}
	explicit Vector(int size, const T &default_value = T()) {
		m_base.alloc(sizeof(T), size, size);
		for(int idx = 0; idx < size; idx++)
			new(((T *)m_base.data) + idx) T(default_value);
	}
	template <class IT, EnableIf<is_input_iter<IT>>...> Vector(IT first, IT last) : Vector() {
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

	void assign(const T *first, const T *last) {
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
		resizePrelude(new_size);
		while(index < new_size)
			new(data() + index++) T(default_value);
	}
	void resize(int new_size) {
		int index = m_base.size;
		resizePrelude(new_size);
		while(index < new_size)
			new(data() + index++) T();
	}
	void shrink(int new_size) {
		PASSERT(m_base.size >= new_size);
		resizePrelude(new_size);
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

	void erase(const T *it) {
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

	void erase(const T *a, const T *b) {
		if(trivial_move_constr && trivial_destruction)
			m_base.erasePod(sizeof(T), a - begin(), b - a);
		else
			m_base.erase(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, a - begin(), b - a);
	}

	T *insert(const T *pos, const T &value) { return insert(pos, &value, (&value) + 1); }

	template <class IT, EnableIf<is_input_iter<IT>>...> T *insert(const T *pos, IT first, IT last) {
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

	T *insert(const T *pos, const T *first, const T *last) {
		if(pos == end() && last - first <= m_base.capacity - m_base.size) {
			if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
				memcpy((void *)end(), (void *)first, (last - first) * sizeof(T));
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

	T *insert(const T *pos, std::initializer_list<T> il) {
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

	template <class U = T, EnableIf<equality_comparable<U, U>>...>
	bool operator==(const Vector &rhs) const {
		return size() == rhs.size() && std::equal(begin(), end(), rhs.begin(), rhs.end());
	}

	template <class U = T, EnableIf<less_comparable<U>>...>
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

	void resizePrelude(int new_size) {
		if(trivial_move_constr && trivial_destruction && trivial_copy_constr)
			m_base.resizePodPartial(sizeof(T), new_size);
		else
			m_base.resizePartial(sizeof(T), &Vector::destroy, &Vector::moveAndDestroy, new_size);
	}

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
