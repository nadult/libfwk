// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/unique_ptr.h"

namespace fwk {

template <class T> struct IsCloneable {
	template <class U> static auto test(const U &) -> decltype(declval<const U &>().clone());
	static char test(...);
	static constexpr bool value = std::is_same<T *, decltype(test(declval<const T &>()))>::value;
};

template <class T> class ClonePtr : public UniquePtr<T> {
  public:
	static_assert(IsCloneable<T>::value, "Object should have method: 'T *clone() const'");

	using UniquePtr<T>::UniquePtr;
	ClonePtr(const UniquePtr<T> &rhs) : UniquePtr<T>(rhs ? rhs->clone() : nullptr) {}
	ClonePtr(const ClonePtr &rhs) : UniquePtr<T>(rhs ? rhs->clone() : nullptr) {}
	ClonePtr(ClonePtr &&rhs) : UniquePtr<T>(move(rhs)) {}
	ClonePtr() {}

	using UniquePtr<T>::operator bool;
	using UniquePtr<T>::get;
	using UniquePtr<T>::reset;

	void operator=(ClonePtr &&rhs) { UniquePtr<T>::operator=(move(rhs)); }
	void operator=(const ClonePtr &rhs) {
		if(&rhs == this)
			return;
		reset(rhs ? rhs->clone() : nullptr);
	}
};
}
