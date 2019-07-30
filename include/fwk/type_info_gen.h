// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include <fwk/type_info.h>
#include <typeinfo>

namespace fwk {

namespace detail {
	void addTypeName(const TypeInfoData &, const char *mangled_name);

	template <class T> struct TypeData {
		static constexpr int getSize() {
			if constexpr(!std::is_void<T>::value)
				return std::is_reference<T>::value ? sizeof(void *) : sizeof(T);
			return 0;
		}

		static constexpr int getAlignment() {
			if constexpr(!std::is_void<T>::value)
				return std::is_reference<T>::value ? alignof(void *) : alignof(T);
			return 0;
		}

		using ConstOrNot = If<is_const<T>, RemoveConst<T>, const T>;

		static constexpr const TypeInfoData *constOrNot() {
			if constexpr(!std::is_reference<T>())
				return &TypeData<ConstOrNot>::data;
			return nullptr;
		}

		static constexpr const TypeInfoData *pointerBase() {
			if constexpr(std::is_pointer<T>())
				return &TypeData<RemovePointer<T>>::data;
			return nullptr;
		}
		static constexpr const TypeInfoData *referenceBase() {
			if constexpr(std::is_reference<T>())
				return &TypeData<RemoveReference<T>>::data;
			return nullptr;
		}

		static constexpr TypeInfoData data = {
			constOrNot(),   pointerBase(), referenceBase(),			  getSize(),
			getAlignment(), is_const<T>,   std::is_volatile<T>::value};
		static inline int reg_name = (addTypeName(data, typeid(T).name()), 0);

		// This is unfortunate, but necessary...
		static void invokeReg() {
			auto dummy1 = reg_name;
			if constexpr(std::is_pointer<T>())
				TypeData<RemovePointer<T>>::invokeReg();
			if constexpr(std::is_reference<T>())
				TypeData<RemoveReference<T>>::invokeReg();
			else
				auto dummy2 = TypeData<ConstOrNot>::reg_name;
		}
	};
}

template <class T> TypeInfo typeInfo() {
	using TData = detail::TypeData<T>;
	TData::invokeReg();
	return {TData::data};
}

template <class T> TypeId typeId() { return typeInfo<T>().id(); }
template <class T> ZStr typeName() { return typeInfo<T>().name(); }

template <class T> bool TypeInfo::is() const { return *this == typeInfo<T>(); }
template <class... Types> bool TypeInfo::isOneOf() const {
	return (... || (*this == typeInfo<Types>()));
}
}
