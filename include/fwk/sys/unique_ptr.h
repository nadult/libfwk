// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <fwk/meta.h>
#include <utility>

namespace fwk {

template <class T> struct HasCloneMethod {
	template <class U> static auto test(const U &) -> decltype(DECLVAL(const U &).clone());
	static char test(...);
	static constexpr bool value = is_same<T *, decltype(test(DECLVAL(const T &)))>;
};

// Improved unique_ptr (unique pointer to object)
// To copy UniquePtr<T> one of the following conditions must be met:
// - T is copy constructible and is not polymorphic
// - T has clone() method which returns pointer to newly allocated T
template <typename T> class UniquePtr {
  public:
	static_assert(!std::is_array<T>::value, "Just use a vector...");

	constexpr UniquePtr() {}
	constexpr UniquePtr(std::nullptr_t) {}
	explicit UniquePtr(T *ptr) : m_ptr(ptr) {}
	UniquePtr(UniquePtr &&x) : m_ptr(x.release()) {}

	template <class U, EnableIf<is_convertible<U *, T *>>...>
	UniquePtr(UniquePtr<U> &&rhs) : m_ptr(rhs.release()) {}

	UniquePtr &operator=(UniquePtr &&rhs) {
		reset(rhs.release());
		return *this;
	}

	UniquePtr(const UniquePtr &rhs) : m_ptr(rhs.clone()) {}
	~UniquePtr() { reset(); }

	template <class U, EnableIf<is_convertible<U *, T *>>...>
	UniquePtr &operator=(UniquePtr<U> &&rhs) {
		reset(rhs.release());
		return *this;
	}
	UniquePtr &operator=(const UniquePtr &rhs) {
		reset(rhs.clone());
		return *this;
	}

	UniquePtr &operator=(std::nullptr_t) {
		reset();
		return *this;
	}
	UniquePtr &operator=(T *ptr) {
		reset(ptr);
		return *this;
	}

	void reset(T *ptr = nullptr) {
		if(ptr != m_ptr) {
			delete(m_ptr);
			m_ptr = ptr;
		}
	}

	template <typename... Args> void emplace(Args &&... args) {
		reset(new T(std::forward<Args>(args)...));
	}
	template <class T1, typename... Args> void emplace(Args &&... args) {
		reset(new T1(std::forward<Args>(args)...));
	}

	T *release() {
		auto ret = m_ptr;
		m_ptr = nullptr;
		return ret;
	}

	void swap(UniquePtr &rhs) { std::swap(m_ptr, rhs.m_ptr); }

	T &operator*() const {
		PASSERT(m_ptr);
		return *m_ptr;
	}
	T *operator->() const {
		PASSERT(m_ptr);
		return m_ptr;
	}
	T *get() const { return m_ptr; }

	explicit operator bool() const { return m_ptr != nullptr; }

  private:
	T *clone() const {
		if constexpr(std::is_copy_constructible<T>::value && !std::is_polymorphic<T>::value) {
			return m_ptr ? new T(*m_ptr) : nullptr;
		} else {
			static_assert(HasCloneMethod<T>::value, "T cannot be copied; Please provide clone() "
													"method or copy constructor (if T is not "
													"polymorphic)");

			if constexpr(HasCloneMethod<T>::value)
				return m_ptr ? m_ptr->clone() : nullptr;
			return nullptr;
		}
	}

	T *m_ptr = nullptr;
};

template <typename T, typename... Args> UniquePtr<T> uniquePtr(Args &&... args) {
	return UniquePtr<T>{new T(std::forward<Args>(args)...)};
}
}
