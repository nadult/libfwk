// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/meta_base.h"

namespace fwk {

template <class T> class MaybeRef {
  public:
	enum { is_const = fwk::is_const<T> };

	MaybeRef(Maybe<T> &rhs) : m_ptr(rhs ? rhs.get() : nullptr) {}
	template <class U = T>
		requires(fwk::is_const<U>)
	MaybeRef(const Maybe<RemoveConst<T>> &rhs) : m_ptr(rhs ? rhs.get() : nullptr) {}

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
