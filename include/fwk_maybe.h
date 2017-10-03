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

// Minor modifications by Krzysztof 'nadult' Jakubowski

#pragma once

#include <type_traits>
#include <utility>

namespace fwk {

namespace detail {
	struct NoneHelper {};
}

typedef int detail::NoneHelper::*None;
const None none = nullptr;

/**
 * gcc-4.7 warns about use of uninitialized memory around the use of storage_
 * even though this is explicitly initialized at each point.
 */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif // __GNUC__

template <class T> class Maybe {
  public:
	typedef T value_type;

	static_assert(!std::is_reference<T>::value, "Maybe may not be used with reference types");
	static_assert(!std::is_abstract<T>::value, "Maybe may not be used with abstract types");

	Maybe() noexcept {}

	Maybe(const Maybe &src) noexcept(std::is_nothrow_copy_constructible<T>::value) {
		if(src.hasValue())
			construct(src.value());
	}

	Maybe(Maybe &&src) noexcept(std::is_nothrow_move_constructible<T>::value) {
		if(src.hasValue()) {
			construct(std::move(src.value()));
			src.clear();
		}
	}

	~Maybe() {
		if constexpr(!std::is_trivially_destructible<T>::value)
			if(m_has_value)
				m_value.~T();
	}

	/* implicit */ Maybe(const None &) noexcept {}

	/* implicit */ Maybe(T &&newValue) noexcept(std::is_nothrow_move_constructible<T>::value) {
		construct(std::move(newValue));
	}

	/* implicit */ Maybe(const T &newValue) noexcept(std::is_nothrow_copy_constructible<T>::value) {
		construct(newValue);
	}

	void assign(const None &) { clear(); }

	void assign(Maybe &&src) {
		if(this != &src) {
			if(src.hasValue()) {
				assign(std::move(src.value()));
				src.clear();
			} else {
				clear();
			}
		}
	}

	void assign(const Maybe &src) {
		if(src.hasValue())
			assign(src.value());
		else
			clear();
	}

	void assign(T &&newValue) {
		if(hasValue())
			m_value = std::move(newValue);
		else
			construct(std::move(newValue));
	}

	void assign(const T &newValue) {
		if(hasValue())
			m_value = newValue;
		else
			construct(newValue);
	}

	template <class Arg> Maybe &operator=(Arg &&arg) {
		assign(std::forward<Arg>(arg));
		return *this;
	}

	Maybe &operator=(Maybe &&other) noexcept(std::is_nothrow_move_assignable<T>::value) {
		assign(std::move(other));
		return *this;
	}

	Maybe &operator=(const Maybe &other) noexcept(std::is_nothrow_copy_assignable<T>::value) {
		assign(other);
		return *this;
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
		require_value();
		return m_value;
	}

	T &value() & {
		require_value();
		return m_value;
	}

	T value() && {
		require_value();
		return std::move(m_value);
	}

	const T *get_pointer() const & { return m_has_value ? &m_value : nullptr; }
	T *get_pointer() & { return m_has_value ? &m_value : nullptr; }
	T *get_pointer() && = delete;

	bool hasValue() const { return m_has_value; }

	explicit operator bool() const { return hasValue(); }

	const T &operator*() const & { return value(); }
	T &operator*() & { return value(); }
	T operator*() && { return std::move(value()); }

	const T *operator->() const { return &value(); }
	T *operator->() { return &value(); }

	// Return a copy of the value if set, or a given default if not.
	template <class U> T value_or(U &&dflt) const & {
		if(m_has_value)
			return m_value;
		return std::forward<U>(dflt);
	}

	template <class U> T value_or(U &&dflt) && {
		if(m_has_value)
			return std::move(m_value);
		return std::forward<U>(dflt);
	}

	bool operator==(const T &rhs) const { return hasValue() && value() == rhs; }

	bool operator==(const Maybe &rhs) const {
		if(hasValue() != rhs.hasValue())
			return false;
		if(hasValue())
			return value() == rhs.value();
		return true;
	}

	bool operator<(const Maybe &rhs) const {
		if(hasValue() != rhs.hasValue())
			return hasValue() < rhs.hasValue();
		if(hasValue())
			return value() < rhs.value();
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

  private:
	void require_value() const {
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

template <class T> const T *get_pointer(const Maybe<T> &opt) { return opt.get_pointer(); }

template <class T> T *get_pointer(Maybe<T> &opt) { return opt.get_pointer(); }

template <class T> void swap(Maybe<T> &a, Maybe<T> &b) {
	if(a.hasValue() && b.hasValue()) {
		// both full
		using std::swap;
		swap(a.value(), b.value());
	} else if(a.hasValue() || b.hasValue()) {
		std::swap(a, b); // fall back to default implementation if they're mixed.
	}
}

template <class T, class Opt = Maybe<typename std::decay<T>::type>> Opt makeMaybe(T &&v) {
	return Opt(std::forward<T>(v));
}

template <class V> bool operator==(const V &a, const Maybe<V> &b) {
	return b.hasValue() && b.value() == a;
}

// Suppress comparability of Maybe<T> with T, despite implicit conversion.
template <class V> bool operator<(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator<=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>(const V &other, const Maybe<V> &) = delete;
}
