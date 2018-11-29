// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/sys/unique_ptr.h"
#include "fwk/sys/xml.h"
#include "fwk/type_info_gen.h"

// TODO: interaction with maybe ?
// TODO: text format ?

namespace fwk {

namespace detail {
	void reportAnyError(TypeInfo requested, TypeInfo current);
	template <class RequestedT> void debugCheckAny(TypeInfo any_type_info) {
#ifndef NDEBUG
		if(any_type_info != typeInfo<RequestedT>())
			reportAnyError(typeInfo<RequestedT>(), any_type_info);
#endif
	}

	struct AnyBase {
		virtual ~AnyBase() = default;
		virtual AnyBase *clone() const = 0;
		virtual void *ptr() const = 0;
	};

	using AnyXmlConstructor = AnyBase *(*)(CXmlNode);
	using AnyXmlSaver = void (*)(const void *, XmlNode);
	void registerAnyType(TypeInfo, AnyXmlConstructor, AnyXmlSaver);

	template <class T> struct AnyModel : public AnyBase {
		AnyModel(T value) : value(move(value)) { auto dummy = reg_dummy; }
		AnyBase *clone() const final { return new AnyModel(value); }
		void *ptr() const { return (void *)&value; }

		T value;

		static AnyXmlConstructor makeConstructor() {
			if constexpr(xml_parsable<T>)
				return [](CXmlNode n) -> AnyBase * { return new AnyModel<T>(parse<T>(n)); };
			return nullptr;
		}
		static AnyXmlSaver makeSaver() {
			if constexpr(xml_saveable<T>)
				return [](const void *v, XmlNode n) { save(n, *(const T *)v); };
			return nullptr;
		}

		static inline int reg_dummy =
			(registerAnyType(typeInfo<T>(), makeConstructor(), makeSaver()), 0);
	};
}

class AnyRef;

class Any {
  public:
	template <class T> using Model = detail::AnyModel<T>;

	template <class T, class DT = Decay<T>>
	Any(T value) : m_model(uniquePtr<Model<DT>>(move(value))), m_type(typeInfo<T>()) {
		static_assert(!is_one_of<DT, Any, AnyRef>);
		static_assert(std::is_copy_constructible<T>::value);
		static_assert(std::is_destructible<T>::value);
	}

	template <class T> Any(Expected<T> value) {
		if(value) {
			m_model = uniquePtr<Model<T>>(move(*value));
			m_type = typeInfo<T>();
		} else {
			m_model = uniquePtr<Model<Error>>(move(value.error()));
			m_type = typeInfo<Error>();
		}
	}

	Any();
	Any(None) : Any() {}

	Any(Expected<Any> &&);
	Any(const Expected<Any> &);

	template <class T> Any(const Maybe<T> &) = delete;

	FWK_COPYABLE_CLASS(Any);

	// TODO: to powinno być dostępne
	Any(const AnyRef &) = delete;

	bool empty() const { return !m_model; }
	auto type() const { return m_type; }
	explicit operator bool() const { return !!m_model; }

	template <class T> bool is() const { return m_type.id() == typeId<T>(); }

	template <class T> const T &get() const {
		detail::debugCheckAny<const T>(m_type.asConst());
		return *(const T *)m_model->ptr();
	}

	template <class T> T &get() {
		detail::debugCheckAny<T>(m_type);
		return *(T *)m_model->ptr();
	}

	template <typename T, EnableIf<!std::is_const<T>::value>...> operator T *() {
		return m_type == typeInfo<T>() ? (T *)m_model->ptr() : nullptr;
	}

	template <typename T> operator const T *() const {
		return m_type.asConst() == typeInfo<const T>() ? (const T *)m_model->ptr() : nullptr;
	}

	// May return error
	static Any tryConstruct(CXmlNode, ZStr type_name);
	static Any tryConstruct(CXmlNode);
	Any(CXmlNode, ZStr type_name);
	Any(CXmlNode);

	// TODO: return expected void ?
	void save(XmlNode node, bool save_type_name = true) const;
	bool xmlEnabled() const;
	void swap(Any &);

  private:
	friend class AnyRef;

	// TODO: Inlined for small types ?
	UniquePtr<detail::AnyBase> m_model;
	TypeInfo m_type;
};
}
