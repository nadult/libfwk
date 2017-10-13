// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_base.h"
#include <atomic>
#include <memory>

namespace fwk {

// TODO: enable_shared_from_this should be hidden
template <class T> class immutable_base : public std::enable_shared_from_this<const T> {
  public:
	immutable_base() : m_mutation_counter(-1) {}
	immutable_base(const immutable_base &) : immutable_base() {}
	immutable_base(immutable_base &&) : immutable_base() {}
	void operator=(const immutable_base &) {}
	void operator=(immutable_base &&) {}

	immutable_ptr<T> get_immutable_ptr() const;

  private:
	std::atomic<int> m_mutation_counter; // TODO: this should be atomic?
	friend class immutable_ptr<T>;
};

// Pointer to immutable object
// TODO: verify that it is thread-safe
// It can be mutated with mutate method, but if the pointer is
// not unique then a copy will be made.
// TODO: make it work without immutable_base
template <class T> class immutable_ptr {
  public:
	immutable_ptr(const T &rhs) : m_ptr(make_shared<const T>(rhs)) { incCounter(); }
	immutable_ptr(T &&rhs) : m_ptr(make_shared<const T>(move(rhs))) { incCounter(); }

	immutable_ptr() = default;
	immutable_ptr(const immutable_ptr &) = default;
	immutable_ptr(immutable_ptr &&) = default;
	immutable_ptr &operator=(const immutable_ptr &) = default;
	immutable_ptr &operator=(immutable_ptr &&) = default;

	const T &operator*() const { return m_ptr.operator*(); }
	const T *operator->() const { return m_ptr.operator->(); }
	const T *get() const { return m_ptr.get(); }

	// It will make a copy if the pointer is not unique
	T *mutate() {
		if(!m_ptr.unique()) {
			m_ptr = make_shared<const T>(*m_ptr.get());
			// TODO: make a copy if counter is too big (and reset the counter)
			incCounter();
		}
		return const_cast<T *>(m_ptr.get());
	}

	explicit operator bool() const { return !!m_ptr.get(); }
	bool operator==(const T *rhs) const { return m_ptr == rhs; }
	bool operator==(const immutable_ptr &rhs) const { return m_ptr == rhs.m_ptr; }
	bool operator<(const immutable_ptr &rhs) const { return m_ptr < rhs.m_ptr; }

	auto getKey() const { return (long long)*m_ptr.get(); }
	auto getWeakPtr() const { return std::weak_ptr<const T>(m_ptr); }

	operator shared_ptr<const T>() const { return m_ptr; }

  private:
	immutable_ptr(shared_ptr<const T> ptr) : m_ptr(move(ptr)) {}
	template <class T1, class... Args> friend immutable_ptr<T1> make_immutable(Args &&...);
	template <class T1> friend immutable_ptr<T1> make_immutable(T1 &&);
	template <class T1, class U>
	friend immutable_ptr<T1> static_pointer_cast(const immutable_ptr<U> &);
	friend class immutable_base<T>;
	friend class immutable_weak_ptr<T>;

	void incCounter() {
		static_assert(std::is_base_of<immutable_base<T>, T>::value, "");
		const immutable_base<T> *base = m_ptr.get();
		const_cast<immutable_base<T> *>(base)->m_mutation_counter++;
	}
	int numMutations() const {
		const immutable_base<T> *base = m_ptr.get();
		return base->m_mutation_counter;
	}

	shared_ptr<const T> m_ptr;
};

template <class T> immutable_ptr<T> immutable_base<T>::get_immutable_ptr() const {
	if(m_mutation_counter >= 0)
		return immutable_ptr<T>(std::enable_shared_from_this<const T>::shared_from_this());
	return immutable_ptr<T>();
}

template <class T> inline T *mutate(immutable_ptr<T> &ptr) { return ptr.mutate(); }

template <class T> immutable_ptr<T> make_immutable(T &&object) {
	auto ret = immutable_ptr<T>(make_shared<const T>(move(object)));
	ret.incCounter();
	return ret;
}

template <class T, class... Args> immutable_ptr<T> make_immutable(Args &&... args) {
	auto ret = immutable_ptr<T>(make_shared<const T>(std::forward<Args>(args)...));
	ret.incCounter();
	return ret;
}

template <class T, class U> immutable_ptr<T> static_pointer_cast(const immutable_ptr<U> &cp) {
	return immutable_ptr<T>(static_pointer_cast<const T>(cp.m_ptr));
}

template <class T> class immutable_weak_ptr {
  public:
	immutable_weak_ptr() = default;
	immutable_weak_ptr(immutable_ptr<T> ptr)
		: m_ptr(ptr.m_ptr), m_mutation_counter(ptr ? ptr.numMutations() : -1) {}

	immutable_ptr<T> lock() const {
		immutable_ptr<T> out(m_ptr.lock());
		if(out && out.numMutations() == m_mutation_counter)
			return out;
		return immutable_ptr<T>();
	}

	bool operator==(const immutable_weak_ptr &rhs) const {
		return !m_ptr.owner_before(rhs.m_ptr) && !rhs.m_ptr.owner_before(m_ptr) &&
			   m_mutation_counter == rhs.m_mutation_counter;
	}
	bool operator<(const immutable_weak_ptr &rhs) const {
		if(m_mutation_counter == rhs.m_mutation_counter)
			return m_ptr.owner_before(rhs.m_ptr);
		return m_mutation_counter < rhs.m_mutation_counter;
	}
	bool expired() const { return m_ptr.expired(); }

  private:
	std::weak_ptr<const T> m_ptr;
	int m_mutation_counter;
};
}
