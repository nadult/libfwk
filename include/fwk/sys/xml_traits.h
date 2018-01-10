// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/parse.h"
#include "fwk/sys/xml.h"
#include "fwk/type_info.h"

namespace fwk {

template <class T> struct XmlTraits {
	template <class C> static auto testC(int) -> decltype(C(std::declval<CXmlNode>()));
	template <class C> static auto testC(...) -> Empty;

	template <class C>
	static auto testS(int) -> decltype(std::declval<const C &>().save(std::declval<XmlNode>()));
	template <class C> static auto testS(...) -> Empty;

	static constexpr bool constructible = !isSame<decltype(testC<T>(0)), Empty>();
	static constexpr bool saveable = !isSame<decltype(testS<T>(0)), Empty>();
	static constexpr bool formattible = isFormattible<T>();
	static constexpr bool parsable = isParsable<T>();
};

// TODO: user can overload construct<>() but type will still be not xml_saveable<>
template <class T>
constexpr bool xml_saveable = XmlTraits<T>::saveable || XmlTraits<T>::formattible;
template <class T>
constexpr bool xml_constructible = XmlTraits<T>::constructible || XmlTraits<T>::parsable;

template <class T, EnableIf<xml_constructible<T>>...> T construct(CXmlNode node) {
	if constexpr(XmlTraits<T>::constructible)
		return T(node);
	else
		return node.value<T>();
}

template <class T, EnableIf<xml_saveable<T>>...> void save(const T &value, XmlNode node) {
	if constexpr(XmlTraits<T>::saveable)
		value.save(node);
	else
		node.setValue(value);
}
}
