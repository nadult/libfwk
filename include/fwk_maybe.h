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

#include <cstddef>
#include <new>
#include <stdexcept>
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

class MaybeEmptyException : public std::runtime_error {
  public:
	MaybeEmptyException() : std::runtime_error("Empty Maybe cannot be unwrapped") {}
};

template <class Value> class Maybe {
  public:
	typedef Value value_type;

	static_assert(!std::is_reference<Value>::value, "Maybe may not be used with reference types");
	static_assert(!std::is_abstract<Value>::value, "Maybe may not be used with abstract types");

	Maybe() noexcept {}

	Maybe(const Maybe &src) noexcept(std::is_nothrow_copy_constructible<Value>::value) {
		if(src.hasValue()) {
			construct(src.value());
		}
	}

	Maybe(Maybe &&src) noexcept(std::is_nothrow_move_constructible<Value>::value) {
		if(src.hasValue()) {
			construct(std::move(src.value()));
			src.clear();
		}
	}

	/* implicit */ Maybe(const None &) noexcept {}

	/* implicit */ Maybe(Value &&newValue) noexcept(
		std::is_nothrow_move_constructible<Value>::value) {
		construct(std::move(newValue));
	}

	/* implicit */ Maybe(const Value &newValue) noexcept(
		std::is_nothrow_copy_constructible<Value>::value) {
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
		if(src.hasValue()) {
			assign(src.value());
		} else {
			clear();
		}
	}

	void assign(Value &&newValue) {
		if(hasValue()) {
			storage_.value = std::move(newValue);
		} else {
			construct(std::move(newValue));
		}
	}

	void assign(const Value &newValue) {
		if(hasValue()) {
			storage_.value = newValue;
		} else {
			construct(newValue);
		}
	}

	template <class Arg> Maybe &operator=(Arg &&arg) {
		assign(std::forward<Arg>(arg));
		return *this;
	}

	Maybe &operator=(Maybe &&other) noexcept(std::is_nothrow_move_assignable<Value>::value) {

		assign(std::move(other));
		return *this;
	}

	Maybe &operator=(const Maybe &other) noexcept(std::is_nothrow_copy_assignable<Value>::value) {

		assign(other);
		return *this;
	}

	template <class... Args> void emplace(Args &&... args) {
		clear();
		construct(std::forward<Args>(args)...);
	}

	void clear() { storage_.clear(); }

	const Value &value() const & {
		require_value();
		return storage_.value;
	}

	Value &value() & {
		require_value();
		return storage_.value;
	}

	Value value() && {
		require_value();
		return std::move(storage_.value);
	}

	const Value *get_pointer() const & { return storage_.hasValue ? &storage_.value : nullptr; }
	Value *get_pointer() & { return storage_.hasValue ? &storage_.value : nullptr; }
	Value *get_pointer() && = delete;

	bool hasValue() const { return storage_.hasValue; }

	explicit operator bool() const { return hasValue(); }

	const Value &operator*() const & { return value(); }
	Value &operator*() & { return value(); }
	Value operator*() && { return std::move(value()); }

	const Value *operator->() const { return &value(); }
	Value *operator->() { return &value(); }

	// Return a copy of the value if set, or a given default if not.
	template <class U> Value value_or(U &&dflt) const & {
		if(storage_.hasValue) {
			return storage_.value;
		}

		return std::forward<U>(dflt);
	}

	template <class U> Value value_or(U &&dflt) && {
		if(storage_.hasValue) {
			return std::move(storage_.value);
		}

		return std::forward<U>(dflt);
	}

  private:
	void require_value() const {
		if(!storage_.hasValue)
			FATAL("Dereferencing empty Maybe");
	}

	template <class... Args> void construct(Args &&... args) {
		const void *ptr = &storage_.value;
		// for supporting const types
		new(const_cast<void *>(ptr)) Value(std::forward<Args>(args)...);
		storage_.hasValue = true;
	}

	struct StorageTriviallyDestructible {
		// uninitialized
		union {
			Value value;
		};
		bool hasValue;

		StorageTriviallyDestructible() : hasValue{false} {}

		void clear() { hasValue = false; }
	};

	struct StorageNonTriviallyDestructible {
		// uninitialized
		union {
			Value value;
		};
		bool hasValue;

		StorageNonTriviallyDestructible() : hasValue{false} {}

		~StorageNonTriviallyDestructible() { clear(); }

		void clear() {
			if(hasValue) {
				hasValue = false;
				value.~Value();
			}
		}
	};

	using Storage = typename std::conditional<std::is_trivially_destructible<Value>::value,
											  StorageTriviallyDestructible,
											  StorageNonTriviallyDestructible>::type;

	Storage storage_;
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

template <class V> bool operator==(const Maybe<V> &a, const V &b) {
	return a.hasValue() && a.value() == b;
}

template <class V> bool operator!=(const Maybe<V> &a, const V &b) { return !(a == b); }

template <class V> bool operator==(const V &a, const Maybe<V> &b) {
	return b.hasValue() && b.value() == a;
}

template <class V> bool operator!=(const V &a, const Maybe<V> &b) { return !(a == b); }

template <class V> bool operator==(const Maybe<V> &a, const Maybe<V> &b) {
	if(a.hasValue() != b.hasValue()) {
		return false;
	}
	if(a.hasValue()) {
		return a.value() == b.value();
	}
	return true;
}

template <class V> bool operator!=(const Maybe<V> &a, const Maybe<V> &b) { return !(a == b); }

template <class V> bool operator<(const Maybe<V> &a, const Maybe<V> &b) {
	if(a.hasValue() != b.hasValue()) {
		return a.hasValue() < b.hasValue();
	}
	if(a.hasValue()) {
		return a.value() < b.value();
	}
	return false;
}

template <class V> bool operator>(const Maybe<V> &a, const Maybe<V> &b) { return b < a; }

template <class V> bool operator<=(const Maybe<V> &a, const Maybe<V> &b) { return !(b < a); }

template <class V> bool operator>=(const Maybe<V> &a, const Maybe<V> &b) { return !(a < b); }

// Suppress comparability of Maybe<T> with T, despite implicit conversion.
template <class V> bool operator<(const Maybe<V> &, const V &other) = delete;
template <class V> bool operator<=(const Maybe<V> &, const V &other) = delete;
template <class V> bool operator>=(const Maybe<V> &, const V &other) = delete;
template <class V> bool operator>(const Maybe<V> &, const V &other) = delete;
template <class V> bool operator<(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator<=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>=(const V &other, const Maybe<V> &) = delete;
template <class V> bool operator>(const V &other, const Maybe<V> &) = delete;
}
