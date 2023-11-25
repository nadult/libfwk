// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include <utility>

namespace fwk {

// fwk::Maybe evolved from folly::Optional
// Link: https://github.com/facebook/folly
// Licensed under Apache 2.0 license (available in extern/)

// sizeof(Maybe<T>) will be equal to sizeof(T) if:
// - T can hold Intrusive::EmptyMaybe()
// - you can provide specialization of detail::EmptyMaybe<> for your type
//   example: enums
namespace detail {
	template <class T, class Enable = EnabledType> struct EmptyMaybe {
		static constexpr None make() { return none; }
		static constexpr bool valid(None) { return false; }
	};

	template <class T>
	struct EmptyMaybe<T, EnableIf<Intrusive::CanHold<T, Intrusive::ETag::empty_maybe>::value>> {
		static constexpr T make() { return T(Intrusive::EmptyMaybe()); }
		static constexpr bool valid(const T &val) { return !val.holds(Intrusive::EmptyMaybe()); }
	};
}

template <class T> constexpr bool is_maybe = false;
template <class T> constexpr bool is_maybe<Maybe<T>> = true;

// Generic storage
template <class T, class Enabled = EnabledType> struct MaybeStorage {
	static_assert(!std::is_reference<T>::value, "Maybe may not be used with reference types");
	static_assert(!std::is_abstract<T>::value, "Maybe may not be used with abstract types");

	MaybeStorage() {}

	MaybeStorage(const MaybeStorage &src) {
		if(src.m_has_value)
			construct(src.m_value);
	}

	MaybeStorage(MaybeStorage &&src) {
		if(src.m_has_value) {
			construct(std::move(src.m_value));
			src.clear();
		}
	}

	~MaybeStorage() {
		if(m_has_value)
			m_value.~T();
	}

	// Implicit constructors:
	MaybeStorage(T &&newValue) { construct(std::move(newValue)); }
	MaybeStorage(const T &newValue) { construct(newValue); }

	void operator=(T &&newValue) {
		if(m_has_value)
			m_value = std::move(newValue);
		else
			construct(std::move(newValue));
	}

	void operator=(const T &newValue) {
		if(m_has_value)
			m_value = newValue;
		else
			construct(newValue);
	}

	void operator=(MaybeStorage &&src) {
		if(this == &src)
			return;

		if(src.m_has_value) {
			*this = std::move(src.m_value);
			src.clear();
		} else {
			clear();
		}
	}

	void operator=(const MaybeStorage &src) {
		if(src.m_has_value)
			*this = src.m_value;
		else
			clear();
	}

	bool hasValue() const { return m_has_value; }

	void swap(MaybeStorage &rhs) {
		if(m_has_value && rhs.m_has_value) {
			// both full
			using std::swap;
			swap(m_value, rhs.m_value);
		} else if(m_has_value || rhs.m_has_value) {
			// fall back to default implementation if they're mixed.
			std::swap(*this, rhs);
		}
	}

  protected:
	void clear() {
		if(m_has_value) {
			m_value.~T();
			m_has_value = false;
		}
	}

	template <class... Args> void construct(Args &&...args) {
		const void *ptr = &m_value;
		// for supporting const types
		new(const_cast<void *>(ptr)) T(std::forward<Args>(args)...);
		m_has_value = true;
	}

	union {
		T m_value; //[[maybe_unused]]
	};
	bool m_has_value = false;
};

// Storage for types constructible with detail::EmptyMaybe()
template <class T>
class MaybeStorage<T, EnableIf<is_same<decltype(detail::EmptyMaybe<T>::make()), T>>> {
  public:
	MaybeStorage() : m_value(Empty::make()) {}
	MaybeStorage(T &&value) : m_value(std::move(value)) {}
	MaybeStorage(const T &value) : m_value(value) {}

	MaybeStorage(const MaybeStorage &src) = default;
	MaybeStorage(MaybeStorage &&src) = default;
	~MaybeStorage() = default;

	bool hasValue() const { return Empty::valid(m_value); }

	void operator=(T &&new_value) { m_value = std::move(new_value); }
	void operator=(const T &new_value) { m_value = new_value; }
	MaybeStorage &operator=(MaybeStorage &&) = default;
	MaybeStorage &operator=(const MaybeStorage &) = default;

	void swap(MaybeStorage &rhs) {
		using std::swap;
		swap(m_value, rhs.m_value);
	}

  protected:
	using Empty = detail::EmptyMaybe<T>;

	T m_value;
};

template <class T> class Maybe : public MaybeStorage<T> {
	using MaybeStorage<T>::m_value;

  public:
	typedef T value_type;

	static_assert(!std::is_reference<T>::value, "Maybe may not be used with reference types");
	static_assert(!std::is_abstract<T>::value, "Maybe may not be used with abstract types");

	Maybe(const None &) {}
	Maybe() {}

	Maybe(Maybe &&rhs) = default;
	Maybe(const Maybe &rhs) = default;
	Maybe(T &&value) : MaybeStorage<T>(std::move(value)) {}
	Maybe(const T &value) : MaybeStorage<T>(value) {}

	Maybe &operator=(const Maybe &rhs) = default;
	Maybe &operator=(Maybe &&rhs) = default;
	void operator=(T &&new_value) { MaybeStorage<T>::operator=(std::move(new_value)); }
	void operator=(const T &new_value) { MaybeStorage<T>::operator=(new_value); }

	using MaybeStorage<T>::hasValue;

	explicit operator bool() const { return hasValue(); }

	const T &value() const & { return requireValue(), m_value; }
	T &value() & { return requireValue(), m_value; }
	T value() && { return requireValue(), std::move(m_value); }

	const T *get() const & { return hasValue() ? &m_value : nullptr; }
	T *get() & { return hasValue() ? &m_value : nullptr; }
	T *get() && = delete;

	template <class U> T orElse(U &&on_empty) const & {
		if(hasValue())
			return m_value;
		return std::forward<U>(on_empty);
	}

	template <class U> T orElse(U &&on_empty) && {
		if(hasValue())
			return std::move(m_value);
		return std::forward<U>(on_empty);
	}

	bool operator==(const T &rhs) const { return hasValue() && m_value == rhs; }

	friend bool operator==(const T &a, const MaybeStorage<T> &b) {
		return b.hasValue() && b.m_value == a;
	}

	bool operator==(const Maybe &rhs) const {
		bool lhs_valid = hasValue(), rhs_valid = rhs.hasValue();
		if(lhs_valid != rhs_valid)
			return false;
		return lhs_valid ? m_value == rhs.m_value : true;
	}

	bool operator<(const Maybe &rhs) const {
		bool lhs_valid = hasValue(), rhs_valid = rhs.hasValue();
		if(lhs_valid != rhs_valid)
			return lhs_valid < rhs_valid;
		return lhs_valid ? m_value < rhs.m_value : false;
	}

	bool operator>(const Maybe &rhs) const { return rhs < *this; }
	bool operator<=(const Maybe &rhs) const { return !(rhs < *this); }
	bool operator>=(const Maybe &rhs) const { return !(*this < rhs); }

	const T &operator*() const & { return value(); }
	T &operator*() & { return value(); }
	T operator*() && { return std::move(value()); }

	const T *operator->() const { return &value(); }
	T *operator->() { return &value(); }

	friend bool operator==(const T &a, const Maybe<T> &b) { return b.hasValue() && b.m_value == a; }

  private:
	void requireValue() const {
#ifndef NDEBUG
		if(!MaybeStorage<T>::hasValue())
			FWK_FATAL("Dereferencing empty Maybe");
#endif
	}
};

template <class T> void swap(Maybe<T> &a, Maybe<T> &b) { return a.swap(b); }

template <class T, class U, EnableIf<is_constructible<T, U>>...>
Maybe<T> maybePass(const Maybe<U> &val) {
	return val ? T(*val) : Maybe<T>();
}

template <class T, class TMaybe = Maybe<Decay<T>>> TMaybe makeMaybe(T &&v) {
	return TMaybe(std::forward<T>(v));
}
}
