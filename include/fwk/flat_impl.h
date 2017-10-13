// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <utility>

namespace fwk {

template <class T, int size, int minimum_size> constexpr inline void validateSize() {
	static_assert(size >= minimum_size, "Invalid size");
}

// T definition is not required, you just have to make sure
// that size is at least as big as sizeof(T)
// It's similar to unique_ptr but it keeps the data in-place
// TODO: better name ?
template <class T, unsigned size> struct FlatImpl {
	template <class... Args> FlatImpl(Args &&... args) {
		validateSize<T, size, sizeof(T)>();
		new(data) T(std::forward<Args>(args)...);
	}
	FlatImpl(const FlatImpl &rhs) { new(data) T(*rhs); }
	FlatImpl(FlatImpl &&rhs) { new(data) T(move(*rhs)); }
	~FlatImpl() { (**this).~T(); }

	void operator=(const FlatImpl &rhs) { **this = *rhs; }
	void operator=(FlatImpl &&rhs) { **this = move(*rhs); }

	const T &operator*() const {
		validateSize<T, size, sizeof(T)>();
		return reinterpret_cast<const T &>(*this);
	}
	T &operator*() {
		validateSize<T, size, sizeof(T)>();
		return reinterpret_cast<T &>(*this);
	}
	T *operator->() { return &operator*(); }
	const T *operator->() const { return &operator*(); }

  private:
	char data[size];
};
}
