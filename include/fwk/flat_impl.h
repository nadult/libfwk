// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta.h"

namespace fwk {

template <class T, int specified, int required> struct InvalidFlatImplSize;
template <class T, int specified, int required> struct InvalidFlatImplAlignment;

template <class T, int size, int align> struct alignas(align) FlatProxy {
	FlatProxy() = delete;
	FlatProxy(const FlatProxy &) = delete;
	void operator=(const FlatProxy &) = delete;

  private:
	char data[size];
};

namespace detail {
	// Doesn't work for private members
	template <class T> struct IsDefined {
		template <class C> static auto test(int) -> IndexedType<C, sizeof(C)>;
		template <class C> static auto test(...) -> void;
		static constexpr bool value = !std::is_same<void, decltype(test<T>(0))>::value;
	};

	template <class T> struct ArgsDefined { static constexpr bool value = true; };
	template <template <class...> class T, class... Args> struct ArgsDefined<T<Args...>> {
		static constexpr bool value = Conjunction<IsDefined<Args>...>::value;
	};

	template <class T, int size, int align, int req_size, int req_align>
	using ValidateFlatImpl = Conditional<
		size != req_size, InvalidFlatImplSize<T, size, req_size>,
		Conditional<align != req_align, InvalidFlatImplAlignment<T, align, req_align>, T>>;

	template <class T, int size, int align, bool defined_params> struct FlatImplSelect {
		template <class C>
		static auto test(int) -> ValidateFlatImpl<C, size, align, sizeof(C), alignof(C)>;
		template <class C> static auto test(...) -> FlatProxy<T, size, align>;
		using type = decltype(test<T>(0));
	};

	template <class T, int size, int align> struct FlatImplSelect<T, size, align, false> {
		using type = FlatProxy<T, size, align>;
	};
}

// 0-overhead Pimpl; When T is defined it evaluates to T, otherwise to FlatProxy<T>;
// You have to exactly know T's size and alignment; in .cpp file you have to make sure
// that T is defined before FlatImpl<T> is instantiated.
// Limitation: it doesn't work for private members.
// Use it for big types which are rarely instantiated.
template <class T, int size = type_size<T>, int alignment = 8>
using FlatImpl =
	typename detail::FlatImplSelect<T, size, alignment, detail::ArgsDefined<T>::value>::type;
}
