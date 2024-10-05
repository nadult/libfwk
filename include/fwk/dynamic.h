// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta_base.h"
#include "fwk/sys_base.h"

namespace fwk {

template <class T> struct HasCloneMethod {
	FWK_SFINAE_TYPE(Result, T, DECLVAL(const U &).clone());
	static constexpr bool value = is_same<T *, Result>;
};

// Dynamic storage: basically it's an unique_ptr with improvements.
// To copy Dynamic<T> one of the following conditions must be met:
// - T is copy constructible and is not polymorphic
// - T has clone() method which returns pointer to newly allocated T
template <typename T> class Dynamic {
  public:
	static_assert(!std::is_array<T>::value, "Just use a vector...");

	constexpr Dynamic() {}
	constexpr Dynamic(std::nullptr_t) {}
	explicit Dynamic(T *ptr) : m_ptr(ptr) {}
	Dynamic(Dynamic &&x) : m_ptr(x.release()) {}

	template <class U>
		requires(is_convertible<U *, T *>)
	Dynamic(Dynamic<U> &&rhs) : m_ptr(rhs.release()) {}

	// At least one argument is required (otherwise default Dynamic constructor will be selected)
	template <class... Args>
		requires(is_constructible<T, Args...>)
	Dynamic(Args &&...args) : m_ptr(new T{std::forward<Args>(args)...}) {}

	Dynamic &operator=(Dynamic &&rhs) {
		reset(rhs.release());
		return *this;
	}

	Dynamic(const Dynamic &rhs) : m_ptr(rhs.clone()) {}
	~Dynamic() { reset(); }

	template <class U>
		requires(is_convertible<U *, T *>)
	Dynamic &operator=(Dynamic<U> &&rhs) {
		reset(rhs.release());
		return *this;
	}
	Dynamic &operator=(const Dynamic &rhs) {
		reset(rhs.clone());
		return *this;
	}

	Dynamic &operator=(std::nullptr_t) {
		reset();
		return *this;
	}
	Dynamic &operator=(T *ptr) {
		reset(ptr);
		return *this;
	}

	void reset(T *ptr = nullptr) {
		if(ptr != m_ptr) {
			delete(m_ptr);
			m_ptr = ptr;
		}
	}

	template <class... Args>
		requires is_constructible<T, Args...>
	void emplace(Args &&...args) {
		reset(new T{std::forward<Args>(args)...});
	}
	template <class T1, class... Args>
		requires is_constructible<T1, Args...>
	void emplace(Args &&...args) {
		reset(new T1(std::forward<Args>(args)...));
	}

	T *release() {
		auto ret = m_ptr;
		m_ptr = nullptr;
		return ret;
	}

	void swap(Dynamic &rhs) { std::swap(m_ptr, rhs.m_ptr); }

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

	bool operator==(const Dynamic &rhs) const {
		if(!m_ptr || !rhs.m_ptr)
			return !!m_ptr == !!rhs.m_ptr;
		return *m_ptr == *rhs.m_ptr;
	}
	bool operator<(const Dynamic &rhs) const {
		if(!m_ptr || !rhs.m_ptr)
			return !!m_ptr < !!rhs.m_ptr;
		return *m_ptr < *rhs.m_ptr;
	}

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

}
