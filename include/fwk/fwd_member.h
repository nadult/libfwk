// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/meta.h"

namespace fwk {

template <class T, int specified, int required> struct InvalidFwdMemberSize;
template <class T, int specified, int required> struct InvalidFwdMemberAlignment;

template <class T, int size, int align> struct alignas(align) FwdMemberMockup {
	FwdMemberMockup() = delete;
	FwdMemberMockup(const FwdMemberMockup &) = delete;
	void operator=(const FwdMemberMockup &) = delete;
	FwdMemberMockup *operator&() const = delete;

  private:
	char data[size];
};

namespace detail {
	// Doesn't work for private members
	template <class T> struct IsDefined {
		template <class C> static auto test(int) -> IndexedType<C, sizeof(C)>;
		template <class C> static auto test(...) -> void;
		static constexpr bool value = !is_same<void, decltype(test<T>(0))>;
	};

	template <class T> struct FullyDefined { static constexpr bool value = IsDefined<T>::value; };
	template <template <class...> class T, class... Args> struct FullyDefined<T<Args...>> {
		static constexpr bool eval() {
			if constexpr(Conjunction<FullyDefined<Args>...>::value)
				return IsDefined<T<Args...>>::value;
			return false;
		};
		static constexpr bool value = eval();
	};

	template <class T, int size, int align, int req_size, int req_align>
	using ValidateFwdMember = Conditional<
		size != req_size, InvalidFwdMemberSize<T, size, req_size>,
		Conditional<align != req_align, InvalidFwdMemberAlignment<T, align, req_align>, T>>;

	template <class T, int size, int align, bool defined_params> struct FwdMemberSelect {
		template <class C>
		static auto test(int) -> ValidateFwdMember<C, size, align, sizeof(C), alignof(C)>;
		template <class C> static auto test(...) -> FwdMemberMockup<T, size, align>;
		using type = decltype(test<T>(0));
	};

	template <class T, int size, int align> struct FwdMemberSelect<T, size, align, false> {
		using type = FwdMemberMockup<T, size, align>;
	};
}

// 0-overhead Pimpl; When T is defined it evaluates to T, otherwise to FwdMemberMockup<T>;
// You have to exactly know T's size and alignment; in .cpp file you have to make sure
// that T is defined before FwdMember<T> is instantiated.
// Limitation: it doesn't work for private members.
// Use it for big types which are rarely instantiated.
template <class T, int size = type_size<T>,
		  int alignment = (size < sizeof(void *) ? size : sizeof(void *))>
using FwdMember =
	typename detail::FwdMemberSelect<T, size, alignment, detail::FullyDefined<T>::value>::type;
}
