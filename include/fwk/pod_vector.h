// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/algorithm.h"
#include "fwk/vector.h"
#include <limits.h>

namespace fwk {

// Very simple and efficent vector for POD Types; Use with care:
// - user is responsible for initialization & destruction of the data
// - resize & reserve will only copy old data, but it won't do any
//   initialization or destruction
// - clear & free won't destroy anything
template <class T> class PodVector {
  public:
	PodVector() { m_base.zero(); }
	explicit PodVector(int size) {
		m_base.zero();
		m_base.resizePodPartial(sizeof(T), size);
	}
	PodVector(CSpan<T> span) : PodVector(span.size()) { copy(data(), span); }
	PodVector(const PodVector &rhs) : PodVector(CSpan<T>(rhs)) {}
	PodVector(PodVector &&rhs) { m_base.moveConstruct(move(rhs.m_base)); }
	~PodVector() { m_base.free(sizeof(T)); }

	void operator=(const PodVector &rhs) {
		if(&rhs == this)
			return;
		resize(rhs.m_base.size);
		copy(data(), rhs);
	}
	void operator=(PodVector &&rhs) {
		if(&rhs == this)
			return;
		m_base.swap(rhs.m_base);
		rhs.free();
	}

	void resize(int new_size) { m_base.resizePodPartial(sizeof(T), new_size); }
	void reserve(int new_capacity) { m_base.reservePod(sizeof(T), new_capacity); }

	void swap(PodVector &rhs) { m_base.swap(rhs.m_base); }
	void unsafeSwap(Vector<T> &rhs) { m_base.swap(rhs.m_base); }

	void clear() { m_base.size = 0; }
	void free() {
		PodVector empty;
		m_base.swap(empty.m_base);
	}

	bool inRange(int idx) const { return fwk::inRange(idx, 0, m_base.size); }

	const T *data() const { return reinterpret_cast<const T *>(m_base.data); }
	T *data() { return reinterpret_cast<T *>(m_base.data); }

	T *begin() { return data(); }
	T *end() { return data() + m_base.size; }
	const T *begin() const { return data(); }
	const T *end() const { return data() + m_base.size; }

	T &operator[](int idx) {
		IF_PARANOID(m_base.checkIndex(idx));
		return data()[idx];
	}
	const T &operator[](int idx) const {
		IF_PARANOID(m_base.checkIndex(idx));
		return data()[idx];
	}

	int size() const { return m_base.size; }
	int capacity() const { return m_base.capacity; }
	i64 usedMemory() const { return m_base.capacity * (i64)sizeof(T); }

	bool empty() const { return m_base.size == 0; }
	explicit operator bool() const { return m_base.size > 0; }

	operator Span<T>() { return Span<T>(data(), m_base.size); }
	operator CSpan<T>() const { return CSpan<T>(data(), m_base.size); }

	template <class U> PodVector<U> reinterpret() {
		static_assert(compatibleSizes(sizeof(T), sizeof(U)),
					  "Incompatible sizes; are you sure, you want to do this cast?");

		m_base.size = (int)(size_t(m_base.size) * sizeof(T) / sizeof(U));
		auto *rcurrent = reinterpret_cast<PodVector<U> *>(this);
		return PodVector<U>(move(*rcurrent));
	}

  private:
	BaseVector m_base;
};
}
