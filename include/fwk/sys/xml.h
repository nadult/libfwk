// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/parse.h"
#include "fwk/sys/unique_ptr.h"

#ifndef RAPIDXML_HPP_INCLUDED

namespace rapidxml {
template <class Ch> class xml_node;
template <class Ch> class xml_document;
};

#endif

namespace fwk {

// Immutable XmlNode
class CXmlNode {
  public:
	CXmlNode(const CXmlNode &) = default;
	CXmlNode() = default;

	// TODO: change to tryAttrib?
	// Returns nullptr if not found
	const char *hasAttrib(const char *name) const;

	// If an attribute cannot be found or properly parsed then exception is thrown
	const char *attrib(const char *name) const;
	// If an attribute cannot be found then default_value will be returned
	const char *attrib(const char *name, const char *default_value) const;

	template <class T> T attrib(const char *name) const { return fromString<T>(attrib(name)); }

	template <class T, class RT = Decay<T>> RT attrib(const char *name, T &&or_else) const {
		const char *value = hasAttrib(name);
		return value ? fromString<RT>(value) : or_else;
	}

	CXmlNode sibling(const char *name = nullptr) const;
	CXmlNode child(const char *name = nullptr) const;

	void next() { *this = sibling(name()); }

	const char *value() const;

	template <class T> T value() const { return fromString<T>(value()); }
	template <class T> T value(T default_value) const {
		const char *val = value();
		return val[0] ? value<T>() : default_value;
	}
	template <class T> T childValue(const char *child_name, T default_value) const {
		CXmlNode child_node = child(child_name);
		const char *val = child_node ? child_node.value() : "";
		return val[0] ? child_node.value<T>() : default_value;
	}

	const char *name() const;
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	CXmlNode(rapidxml::xml_node<char> *ptr) : m_ptr(ptr) {}
	friend class XmlDocument;
	friend class XmlNode;

	rapidxml::xml_node<char> *m_ptr = nullptr;
};

class XmlNode : public CXmlNode {
  public:
	explicit XmlNode(CXmlNode);
	XmlNode(const XmlNode &) = default;
	XmlNode() = default;

	// When adding new nodes, you have to make sure that strings given as
	// arguments will exist as long as XmlNode exists; use 'own' method
	// to reallocate them in the memory pool if you're not sure
	void addAttrib(const char *name, const char *value);
	void addAttrib(const char *name, int value);
	void addAttrib(const char *name, Str value);

	template <class T> void addAttrib(const char *name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		addAttrib(name, own(formatter));
	}
	template <class T> void addAttrib(const char *name, const T &value, const T &default_value) {
		if(value != default_value)
			addAttrib(name, value);
	}

	XmlNode addChild(const char *name, const char *value = nullptr);
	XmlNode sibling(const char *name = nullptr) const;
	XmlNode child(const char *name = nullptr) const;

	template <class T> XmlNode addChild(const char *name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		return addChild(name, own(formatter));
	}

	void setValue(const char *text);

	template <class T> void setValue(const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		setValue(own(formatter));
	}

	const char *own(Str);
	const char *own(const TextFormatter &str) { return own(str.text()); }
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	XmlNode(rapidxml::xml_node<char> *ptr, rapidxml::xml_document<char> *doc)
		: CXmlNode(ptr), m_doc(doc) {}
	friend class XmlDocument;

	rapidxml::xml_document<char> *m_doc = nullptr;
};

class XmlDocument {
  public:
	XmlDocument();
	XmlDocument(Str file_name);
	XmlDocument(Stream &);
	XmlDocument(XmlDocument &&);
	~XmlDocument();
	XmlDocument &operator=(XmlDocument &&);

	void load(Str file_name);
	void save(Str file_name) const;

	void load(Stream &);
	void save(Stream &) const;

	XmlNode addChild(const char *name, const char *value = nullptr);

	// TODO: const should return CXmlNode
	XmlNode child(const char *name = nullptr) const;
	XmlNode child(const string &name) const { return child(name.c_str()); }

	const char *own(Str);
	const char *own(const TextFormatter &str) { return own(str.text()); }

	string lastNodeInfo() const;

  protected:
	UniquePtr<rapidxml::xml_document<char>> m_ptr;
	Str m_xml_string;
};

// Use it to get information about currently parsed XML document on error
class XmlOnFailGuard {
  public:
	XmlOnFailGuard(const XmlDocument &);
	~XmlOnFailGuard();

	const XmlDocument &m_document;
};

namespace detail {
	template <class T> struct XmlTraits {
		template <class C> static auto testC(int) -> decltype(C(declval<CXmlNode>()));
		template <class C> static auto testC(...) -> Empty;

		template <class C>
		static auto testS(int) -> decltype(declval<const C &>().save(declval<XmlNode>()));
		template <class C> static auto testS(...) -> Empty;

		static constexpr bool constructible = !isSame<decltype(testC<T>(0)), Empty>();
		static constexpr bool saveable = !isSame<decltype(testS<T>(0)), Empty>();

		template <class C>
		static auto testFP(int) -> decltype(parse(declval<CXmlNode>(), Type<C>()));
		template <class C> static auto testFP(...) -> Empty;

		template <class C>
		static auto testFS(int) -> decltype(save(declval<XmlNode>(), declval<const C &>()));
		template <class C> static auto testFS(...) -> Empty;

		static constexpr bool func_parsable = !isSame<decltype(testFP<T>(0)), Empty>();
		static constexpr bool func_saveable = !isSame<decltype(testFS<T>(0)), Empty>();
	};
}

// To make type xml_saveable, you have to satisfy one of these conditions:
// - provide save(XmlNode, const T&)
// - provide T::save(XmlNode) const
// - make sure that T is formattible
template <class T>
constexpr bool xml_saveable =
	detail::XmlTraits<T>::saveable || formattible<T> || detail::XmlTraits<T>::func_saveable;

// To make type xml_constructible, you have to satisfy one of these conditions:
// - provide constructor T(CXmlNode)
// - provide parse(CXmlNode, Type<T>, CXmlNode) -> T
// - make sure that T is parsable
template <class T>
constexpr bool xml_parsable =
	detail::XmlTraits<T>::constructible || parsable<T> || detail::XmlTraits<T>::func_parsable;

template <class T, EnableIf<xml_parsable<T>>...> T parse(CXmlNode node) {
	if constexpr(detail::XmlTraits<T>::func_parsable)
		return parse(node, Type<T>());
	else if constexpr(detail::XmlTraits<T>::constructible)
		return T(node);
	else
		return node.value<T>();
}

template <class T, EnableIf<detail::XmlTraits<T>::saveable || formattible<T>>...>
void save(XmlNode node, const T &value) {
	if constexpr(detail::XmlTraits<T>::saveable)
		value.save(node);
	else
		node.setValue(value);
}
}
