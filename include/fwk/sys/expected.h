// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/error.h"

namespace fwk {

template <class T> class Expected {
  public:
	static_assert(!isSame<T, Error>());

	Expected(const T &value) : m_value(value), m_has_value(true) {}
	Expected(T &&value) : m_value(move(value)), m_has_value(true) {}
	Expected(Error error) : m_error(move(error)), m_has_value(false) {}
	~Expected() {
		if(m_has_value)
			m_value.~T();
		else
			m_error.~Error();
	}

	Expected(const Expected &rhs) : m_has_value(rhs.m_has_value) {
		if(m_has_value)
			new(&m_value) T(rhs.m_value);
		else
			new(&m_error) Error(rhs.m_error);
	}
	Expected(Expected &&rhs) : m_has_value(rhs.m_has_value) {
		if(m_has_value)
			new(&m_value) T(move(rhs.m_value));
		else
			new(&m_error) Error(move(rhs.m_error));
	}

	void swap(Expected &rhs) {
		Expected temp = move(*this);
		*this = move(rhs);
		rhs = move(temp);
	}

	void operator=(Expected &&rhs) {
		if(&rhs != this) {
			this->~Expected();
			new(this) Expected(move(rhs));
		}
	}

	void operator=(const Expected &rhs) {
		Expected copy(rhs);
		swap(copy);
	}

	explicit operator bool() const { return m_has_value; }

	T *operator->() {
		DASSERT(m_has_value);
		return &m_value;
	}
	const T *operator->() const {
		DASSERT(m_has_value);
		return &m_value;
	}

	T &operator*() {
		DASSERT(m_has_value);
		return m_value;
	}
	const T &operator*() const {
		DASSERT(m_has_value);
		return m_value;
	}
	const Error &error() const {
		DASSERT(!m_has_value);
		return m_error;
	}

	const T &orElse(const T &on_error) const { return m_has_value ? m_value : on_error; }

  private:
	union {
		Error m_error;
		T m_value;
	};
	bool m_has_value;
};

template <> class Expected<void> {
  public:
	Expected() : m_has_error(false) {}
	Expected(Error error) : m_error(move(error)), m_has_error(true) {}

	void swap(Expected &rhs) {
		fwk::swap(m_has_error, rhs.m_has_error);
		fwk::swap(m_error, rhs.m_error);
	}

	const Error &error() const {
		DASSERT(m_has_error);
		return m_error;
	}

	explicit operator bool() const { return !m_has_error; }

  private:
	Error m_error;
	bool m_has_error;
};
}
