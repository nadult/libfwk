// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include "fwk_range.h"
#include "fwk_vector.h"
#include <limits.h>

namespace fwk {

class Stream;

// Very simple and efficent vector for POD Types; Use with care:
// - user is responsible for initialization of the data (when constructing and resizing)
template <class T> class PodVector {
  public:
	PodVector() { m_base.zero(); }
	explicit PodVector(int size) {
		m_base.zero();
		m_base.resizePodPartial(sizeof(T), size);
	}
	PodVector(CSpan<T> span) : PodVector(span.size()) { memcpy(data(), span.data(), byteSize()); }
	PodVector(const PodVector &rhs) : PodVector(CSpan<T>(rhs)) {}
	PodVector(PodVector &&rhs) { m_base.moveConstruct(move(rhs.m_base)); }
	~PodVector() {}

	void operator=(const PodVector &rhs) {
		if(&rhs == this)
			return;
		resize(rhs.m_base.size);
		memcpy(data(), rhs.data(), byteSize());
	}
	void operator=(PodVector &&rhs) {
		if(&rhs == this)
			return;
		m_base.swap(rhs.m_base);
		rhs.free();
	}
	void load(Stream &sr, int max_size = INT_MAX) { m_base.loadPod(sizeof(T), sr, max_size); }
	void save(Stream &sr) const { m_base.savePod(sizeof(T), sr); }
	void resize(int new_size) { m_base.resizePodPartial(sizeof(T), new_size); }

	void swap(PodVector &rhs) { m_base.swap(rhs.m_base); }
	void unsafeSwap(Vector<T> &rhs) { m_base.swap(rhs.m_swap); }

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
		PASSERT(inRange(idx));
		return data()[idx];
	}
	const T &operator[](int idx) const {
		PASSERT(inRange(idx));
		return data()[idx];
	}

	int size() const { return m_base.size; }
	long long byteSize() const { return m_base.size * (long long)sizeof(T); }
	int capacity() const { return m_base.capacity; }
	bool empty() const { return m_base.size == 0; }

	operator Span<T>() { return {data(), m_base.size}; }
	operator CSpan<T>() const { return {data(), m_base.size}; }

  private:
	BaseVector m_base;
};
}
