/*
 * Copyright 2016 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Some modifications by Krzysztof 'nadult' Jakubowski

#pragma once

#include <type_traits>
#include <utility>

namespace fwk {

namespace detail {
	struct NoneHelper {};
}

typedef int detail::NoneHelper::*None;
const None none = nullptr;

// gcc-4.7 warns about use of uninitialized memory around the use of storage_
// even though this is explicitly initialized at each point.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

template <class T> class Maybe {
  public:
	typedef T value_type;

	static_assert(!std::is_reference<T>::value, "Maybe may not be used with reference types");
	static_assert(!std::is_abstract<T>::value, "Maybe may not be used with abstract types");

	Maybe() {}

	Maybe(const Maybe &src) {
		if(src.m_has_value)
			construct(src.value());
	}

	Maybe(Maybe &&src) {
		if(src.m_has_value) {
			construct(move(src.value()));
			src.clear();
		}
	}

	~Maybe() {
		if constexpr(!std::is_trivially_destructible<T>::value)
			if(m_has_value)
				m_value.~T();
	}

	// Implicit constructors:
	Maybe(const None &) {}
	Maybe(T &&newValue) { construct(move(newValue)); }
	Maybe(const T &newValue) { construct(newValue); }

	void operator=(T &&newValue) {
		if(m_has_value)
			m_value = move(newValue);
		else
			construct(move(newValue));
	}

	void operator=(const T &newValue) {
		if(m_has_value)
			m_value = newValue;
		else
			construct(newValue);
	}

	void operator=(const None &) { clear(); }

	void operator=(Maybe &&src) {
		if(this == &src)
			return;

		if(src.m_has_value) {
			*this = move(src.value());
			src.clear();
		} else {
			clear();
		}
	}

	void operator=(const Maybe &src) {
		if(src.m_has_value)
			*this = src.value();
		else
			clear();
	}

	template <class... Args> void emplace(Args &&... args) {
		clear();
		construct(std::forward<Args>(args)...);
	}

	void clear() {
		if constexpr(!std::is_trivially_destructible<T>::value) {
			if(m_has_value) {
				m_value.~T();
				m_has_value = false;
			}
		} else {
			m_has_value = false;
		}
	}

	const T &value() const & {
		requireValue();
		return m_value;
	}

	T &value() & {
		requireValue();
		return m_value;
	}

	T value() && {
		requireValue();
		return move(m_value);
	}

	const T *get() const & { return m_has_value ? &m_value : nullptr; }
	T *get() & { return m_has_value ? &m_value : nullptr; }
	T *get() && = delete;

	explicit operator bool() const { return m_has_value; }

	const T &operator*() const & { return value(); }
	T &operator*() & { return value(); }
	T operator*() && { return move(value()); }

	const T *operator->() const { return &value(); }
	T *operator->() { return &value(); }

	// Return a copy of the value if set, or a given default if not.
	template <class U> T orElse(U &&on_empty) const & {
		if(m_has_value)
			return m_value;
		return std::forward<U>(on_empty);
	}

	template <class U> T orElse(U &&on_empty) && {
		if(m_has_value)
			return move(m_value);
		return std::forward<U>(on_empty);
	}

	bool operator==(const T &rhs) const { return m_has_value && m_value == rhs; }

	bool operator==(const Maybe &rhs) const {
		if(m_has_value != rhs.m_has_value)
			return false;
		if(m_has_value)
			return m_value == rhs.m_value;
		return true;
	}

	bool operator<(const Maybe &rhs) const {
		if(m_has_value != rhs.m_has_value)
			return m_has_value < rhs.m_has_value;
		if(m_has_value)
			return m_value < rhs.m_value;
		return false;
	}

	bool operator>(const Maybe &rhs) const { return rhs < *this; }
	bool operator<=(const Maybe &rhs) const { return !(rhs < *this); }
	bool operator>=(const Maybe &rhs) const { return !(*this < rhs); }

	// Suppress comparability of Maybe<T> with T, despite implicit conversion.
	bool operator<(const T &other) = delete;
	bool operator<=(const T &other) = delete;
	bool operator>=(const T &other) = delete;
	bool operator>(const T &other) = delete;

	friend bool operator==(const T &a, const Maybe<T> &b) {
		return b.m_has_value && b.m_value == a;
	}

	void swap(Maybe &rhs) {
		if(m_has_value && rhs.m_has_value) {
			// both full
			using std::swap;
			swap(m_value, rhs.m_value);
		} else if(m_has_value || rhs.m_has_value) {
			// fall back to default implementation if they're mixed.
			std::swap(*this, rhs);
		}
	}

  private:
	void requireValue() const {
#ifndef NDEBUG
		if(!m_has_value)
			FATAL("Dereferencing empty Maybe");
#endif
	}

	template <class... Args> void construct(Args &&... args) {
		const void *ptr = &m_value;
		// for supporting const types
		new(const_cast<void *>(ptr)) T(std::forward<Args>(args)...);
		m_has_value = true;
	}

	// uninitialized
	union {
		T m_value;
	};
	bool m_has_value = false;
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <class T> void swap(Maybe<T> &a, Maybe<T> &b) { return a.swap(b); }

template <class T, class Opt = Maybe<typename std::decay<T>::type>> Opt makeMaybe(T &&v) {
	return Opt(std::forward<T>(v));
}

// Suppress comparability of Maybe<T> with T, despite implicit conversion.
template <class V> bool operator<(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator<=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>(const V &other, const Maybe<V> &) = delete;
}
