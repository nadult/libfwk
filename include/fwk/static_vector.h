// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/span.h"

namespace fwk {

template <class T, int max_size_> class StaticVector {
  public:
	static constexpr int max_size = max_size_;
	using IndexType = If<(max_size > 256) || alignof(T) >= sizeof(u32), u32, u8>;
	using value_type = T;

#define DATA ((T *)(m_data))
#define RHS_DATA ((T *)(rhs.m_data))

	StaticVector() : m_size(0) {}
	~StaticVector() {
		for(auto &elem : *this)
			elem.~T();
	}
	StaticVector(const StaticVector &rhs) : m_size(rhs.m_size) {
		for(uint n = 0; n < m_size; n++)
			new(&DATA[n]) T(RHS_DATA[n]);
	}
	// TODO: similar initializer as in EnumMap<>
	StaticVector(CSpan<T> span) : m_size(span.size()) {
		PASSERT(span.size() <= max_size);
		for(uint n = 0; n < m_size; n++)
			new(&DATA[n]) T(span[n]);
	}

	StaticVector(StaticVector &&rhs) : m_size(rhs.m_size) {
		m_size = rhs.m_size;
		for(uint n = 0; n < m_size; n++) {
			new(&DATA[n]) T(move(RHS_DATA[n]));
			RHS_DATA[n].~T();
		}
		rhs.m_size = 0;
	}
	operator CSpan<T>() const { return {data(), size()}; }
	template <class URange, class U = RangeBase<URange>, EnableIf<is_convertible<U, T>>...>
	StaticVector(const URange &range) : StaticVector() {
		PASSERT(fwk::size(range) <= max_size);
		for(auto &elem : range)
			emplace_back(elem);
	}

	void swap(StaticVector &rhs) {
		StaticVector temp = move(*this);
		*this = move(rhs);
		rhs = move(temp);
	}

	// TODO: insert...

	void operator=(StaticVector &&rhs) {
		if(&rhs != this) {
			this->~StaticVector();
			new(this) StaticVector(move(rhs));
		}
	}

	void operator=(const StaticVector &rhs) {
		StaticVector copy(rhs);
		swap(copy);
	}

	template <class... Args> void emplace_back(Args &&... args) {
		PASSERT(m_size < max_size);
		new(&DATA[m_size++]) T{std::forward<Args>(args)...};
	}

	void pop_back() {
		PASSERT(!empty());
		DATA[m_size--].~T();
	}

	bool inRange(int index) const { return index >= 0 && index < size(); }
	bool empty() const { return m_size == 0; }
	explicit operator bool() const { return m_size != 0; }

	void clear() {
		for(auto &elem : *this)
			elem.~T();
		m_size = 0;
	}

	T &operator[](int index) { return PASSERT(inRange(index)), DATA[index]; }
	const T &operator[](int index) const { return PASSERT(inRange(index)), DATA[index]; }

	const T &back() const { return (*this)[size() - 1]; }
	T &back() { return (*this)[size() - 1]; }
	const T &front() const { return (*this)[0]; }
	T &front() { return (*this)[0]; }

	T *data() { return DATA; }
	T *begin() { return DATA; }
	T *end() { return DATA + m_size; }

	const T *data() const { return DATA; }
	const T *begin() const { return DATA; }
	const T *end() const { return DATA + m_size; }

	int size() const { return (int)(m_size); }
	int capacity() const { return max_size; }

	void shrink(int new_size) {
		PASSERT(new_size >= 0 && new_size <= size());
		while(m_size > new_size)
			DATA[m_size--].~T();
	}

	void resize(int new_size) {
		int index = size();
		while(index > new_size)
			DATA[--index].~T();
		while(index < new_size)
			new(data() + index++) T();
		m_size = new_size;
	}

	bool operator==(const StaticVector &rhs) const {
		return size() == rhs.size() && std::equal(begin(), end(), rhs.begin(), rhs.end());
	}

	bool operator<(const StaticVector &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

#undef DATA
#undef RHS_DATA

  private:
	IndexType m_size;
	std::aligned_storage_t<sizeof(T), alignof(T)> m_data[max_size];
};

}
