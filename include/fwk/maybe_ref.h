// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta.h"

namespace fwk {

template <class T> class MaybeRef {
  public:
	MaybeRef(T &ref) : m_ptr(&ref) {}
	MaybeRef(T *ptr) : m_ptr(ptr) {}
	MaybeRef(None) {}
	MaybeRef() = default;

	explicit operator bool() const { return m_ptr; }

	T *get() const { return m_ptr; }

	T &value() const {
		requireValue();
		return *m_ptr;
	}

	T &operator*() const { return value(); }
	T *operator->() const { return &value(); }

	bool operator==(const MaybeRef &rhs) const { return m_ptr; }
	bool operator<(const MaybeRef &rhs) const { return m_ptr; }

  private:
	void requireValue() const {
#ifndef NDEBUG
		if(!m_ptr)
			FWK_FATAL("Dereferencing empty MaybeRef");
#endif
	}

	T *m_ptr = nullptr;
};
}
