#pragma once

#include <fwk/meta.h>
#include <utility>

namespace fwk {

template <typename T> class UniquePtr {
  public:
	static_assert(!std::is_array<T>::value, "Just use a vector...");

	constexpr UniquePtr() {}
	constexpr UniquePtr(std::nullptr_t) {}
	explicit UniquePtr(T *ptr) : m_ptr(ptr) {}
	UniquePtr(UniquePtr &&x) : m_ptr(x.release()) {}

	template <class U, EnableIf<std::is_convertible<U *, T *>::value>...>
	UniquePtr(UniquePtr<U> &&rhs) : m_ptr(rhs.release()) {}

	UniquePtr &operator=(UniquePtr &&rhs) {
		reset(rhs.release());
		return *this;
	}

	template <class U, EnableIf<std::is_convertible<U *, T *>::value>...>
	UniquePtr &operator=(UniquePtr<U> &&rhs) {
		reset(rhs.release());
		return *this;
	}

	UniquePtr &operator=(std::nullptr_t) {
		reset();
		return *this;
	}

	~UniquePtr() { reset(); }

	UniquePtr(const UniquePtr &) = delete;
	UniquePtr &operator=(const UniquePtr &) = delete;
	UniquePtr &operator=(T *) = delete;

	void reset(T *ptr = nullptr) {
		if(ptr != m_ptr) {
			delete(m_ptr);
			m_ptr = ptr;
		}
	}

	T *release() {
		auto ret = m_ptr;
		m_ptr = nullptr;
		return ret;
	}

	void swap(UniquePtr &rhs) { std::swap(m_ptr, rhs.m_ptr); }

	T &operator*() const { return *m_ptr; }
	T *operator->() const { return m_ptr; }
	T *get() const { return m_ptr; }

	explicit operator bool() const { return m_ptr != nullptr; }

  private:
	T *m_ptr = nullptr;
};

template <typename T, typename... Args> UniquePtr<T> uniquePtr(Args &&... args) {
	return UniquePtr<T>{new T(std::forward<Args>(args)...)};
}
}
