// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/io/xml.h"
#include "fwk/sys/expected.h"
#include "fwk/type_info_gen.h"
#include "fwk/variant.h"

// TODO: interaction with Maybe ?
// TODO: conversion to Expected<T> and Expected<Any> ?
// TODO: text format ?

namespace fwk {

namespace detail {
	[[noreturn]] void reportAnyError(TypeInfo requested, TypeInfo current);
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

	using AnyXmlLoader = Ex<AnyBase *> (*)(CXmlNode);
	using AnyXmlSaver = void (*)(const void *, XmlNode);
	void registerAnyType(TypeInfo, AnyXmlLoader, AnyXmlSaver);

	template <class T> struct AnyModel : public AnyBase {
		AnyModel(T value) : value(std::move(value)) { [[maybe_unused]] auto dummy = reg_dummy; }
		AnyBase *clone() const final { return new AnyModel(value); }
		void *ptr() const { return (void *)&value; }

		T value;

		static AnyXmlLoader makeLoader() {
			if constexpr(is_xml_loadable<T>)
				return [](CXmlNode n) -> Ex<AnyBase *> {
					if(auto result = load<T>(n))
						return new AnyModel<T>(std::move(*result));
					else
						return result.error();
				};
			return nullptr;
		}
		static AnyXmlSaver makeSaver() {
			if constexpr(is_xml_saveable<T>)
				return [](const void *v, XmlNode n) { save(n, *(const T *)v); };
			return nullptr;
		}

		static inline int reg_dummy =
			(registerAnyType(typeInfo<T>(), makeLoader(), makeSaver()), 0);
	};
}

// Can store any kind of value
// Supports serialization to/from XML.
// Cooperates with Expected<> and Variant<> (values are unpacked).
class Any {
  public:
	template <class T> using Model = detail::AnyModel<T>;

	template <class T, class DT = Decay<T>> Any(T value) { emplace(std::move(value)); }

	// Error or value will be stored directly
	template <class T> Any(Ex<T> value) {
		if(value) {
			m_model.emplace<Model<T>>(std::move(*value));
			m_type = typeInfo<T>();
		} else {
			m_model.emplace<Model<Error>>(std::move(value.error()));
			m_type = typeInfo<Error>();
		}
	}

	// Variants will be unpacked
	template <class... T> Any(const Variant<T...> &var) {
		var.visit([&](const auto &val) { emplace(val); });
	}

	Any();
	Any(None) : Any() {}

	// Error will be stored directly
	Any(Ex<Any> &&);
	Any(const Ex<Any> &);

	template <class T> Any(const Maybe<T> &) = delete;
	Any(CXmlNode) = delete;
	Any(XmlNode) = delete;

	FWK_COPYABLE_CLASS(Any);

	bool empty() const { return !m_model; }
	auto type() const { return m_type; }
	explicit operator bool() const { return !!m_model; }

	template <class T> bool is() const { return m_type.id() == typeId<T>(); }
	template <class... T> bool isOneOf() const { return (... || (m_type == typeId<T>())); }

	template <class T> Maybe<T> getMaybe() const {
		if constexpr(is_variant<T>) {
			T out;
			if(getVariant_(out))
				return out;
		} else {
			if(m_type == typeInfo<T>())
				return *(T *)m_model->ptr();
		}
		return none;
	}

	template <class T, EnableIf<!is_variant<T>>...> const T &get() const {
		detail::debugCheckAny<const T>(m_type.asConst());
		return *(const T *)m_model->ptr();
	}

	template <class T, EnableIf<!is_variant<T>>...> T &get() {
		detail::debugCheckAny<T>(m_type);
		return *(T *)m_model->ptr();
	}

	template <typename T, EnableIf<!is_const<T> && !is_variant<T>>...> operator T *() {
		return m_type == typeInfo<T>() ? (T *)m_model->ptr() : nullptr;
	}

	template <typename T, EnableIf<!is_variant<T>>...> operator const T *() const {
		return m_type.asConst() == typeInfo<const T>() ? (const T *)m_model->ptr() : nullptr;
	}

	const void *data() const { return m_model->ptr(); }

	static Ex<Any> load(CXmlNode, ZStr type_name);
	static Ex<Any> load(CXmlNode, TypeInfo);
	static Ex<Any> load(CXmlNode);
	void save(XmlNode node, bool save_type_name = true) const;
	bool xmlEnabled() const;

	void swap(Any &);

  private:
	template <class T, class DT = Decay<T>> void emplace(T value) {
		m_type = typeInfo<T>();
		m_model.emplace<Model<DT>>(std::move(value));
		static_assert(!is_one_of<DT, Any>);
		static_assert(std::is_copy_constructible<T>::value);
		static_assert(std::is_destructible<T>::value);
	}

	template <class... T> bool getVariant_(Variant<T...> &out) const {
		if(empty())
			return false;
		void *ptr = m_model->ptr();
		return (... || (m_type == typeInfo<Decay<T>>() ? ((out = *(Decay<T> *)ptr), true) : false));
	}

	// TODO: Inlined for small types ?
	Dynamic<detail::AnyBase> m_model;
	TypeInfo m_type;
};
}
