// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/any.h"

namespace fwk {

class AnyRef {
  public:
	AnyRef(const Any &);
	AnyRef();
	FWK_COPYABLE_CLASS(AnyRef);

	template <class T> AnyRef(const T &ref) : m_ptr((void *)&ref), m_type(typeInfo<T>()) {
		static_assert(!is_one_of<T, Any, AnyRef>);
		auto dummy = detail::AnyModel<T>::reg_dummy;
	}

	auto type() const { return m_type; }
	bool empty() const { return m_ptr != nullptr; }
	explicit operator bool() const { return m_ptr; }

	template <class T> bool is() const { return m_type == typeInfo<T>(); }
	const void *data() const { return m_ptr; }

	template <class T> T &get() {
		detail::debugCheckAny<T>(m_type);
		return *(T *)m_ptr;
	}

	template <class T> const T &get() const {
		detail::debugCheckAny<const T>(m_type.asConst());
		return *(const T *)m_ptr;
	}

	template <typename T, EnableIf<!is_const<T>>...> operator T *() {
		return m_type == typeInfo<T>() ? (T *)m_ptr : nullptr;
	}

	template <typename T> operator const T *() const {
		return m_type.asConst() == typeInfo<const T>() ? (const T *)m_ptr : nullptr;
	}

	// Object has to be xmlEnabled()
	void save(XmlNode, bool save_type_name = true) const;
	bool xmlEnabled() const;

	AnyRef reinterpret(TypeInfo other, NoAssertsTag) const { return {m_ptr, other}; }
	AnyRef reinterpret(TypeInfo other) const {
		DASSERT(other.size() == m_type.size());
		return reinterpret(other, no_asserts);
	}

  private:
	AnyRef(const void *, TypeInfo);
	friend class Any;

	const void *m_ptr;
	TypeInfo m_type;
};
}
