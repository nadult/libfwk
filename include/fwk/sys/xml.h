// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/format.h"
#include "fwk/parse.h"

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
	// Returns empty string if not found
	ZStr hasAttrib(Str name) const;

	// If an attribute cannot be found or properly parsed then error will be registered
	ZStr attrib(Str name) const EXCEPT;
	// If an attribute cannot be found then default_value will be returned
	ZStr attrib(Str name, ZStr default_value) const;
	ZStr attrib(Str name, const char *default_value) const {
		return attrib(name, ZStr(default_value));
	}

	template <class T> T attrib(Str name) const EXCEPT { return fromString<T>(attrib(name)); }

	template <class T, class RT = Decay<T>> RT attrib(Str name, T &&or_else) const EXCEPT {
		ZStr value = hasAttrib(name);
		return value ? fromString<RT>(value) : or_else;
	}

	CXmlNode sibling(Str name = {}) const;
	CXmlNode child(Str name = {}) const;

	void next() { *this = sibling(name()); }

	ZStr value() const;

	template <class T> T value() const EXCEPT { return fromString<T>(value()); }
	template <class T> T value(T default_value) const EXCEPT {
		ZStr val = value();
		return val ? value<T>() : default_value;
	}
	template <class T> T childValue(Str child_name, T default_value) const EXCEPT {
		CXmlNode child_node = child(child_name);
		ZStr val = child_node ? child_node.value() : "";
		return val ? child_node.value<T>() : default_value;
	}

	ZStr name() const;
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
	void addAttrib(Str name, Str value);
	void addAttrib(Str name, int value);

	template <class T> void addAttrib(Str name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		addAttrib(name, own(formatter));
	}
	template <class T> void addAttrib(Str name, const T &value, const T &default_value) {
		if(value != default_value)
			addAttrib(name, value);
	}

	static bool validNodeName(Str);
	XmlNode addChild(Str name, Str value = {});
	XmlNode sibling(Str name = {}) const;
	XmlNode child(Str name = {}) const;

	template <class T> XmlNode addChild(Str name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		return addChild(name, own(formatter));
	}

	void setValue(Str text);

	template <class T> void setValue(const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		setValue(own(formatter));
	}

	Str own(Str);
	Str own(const TextFormatter &str) { return own(str.text()); }
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	XmlNode(rapidxml::xml_node<char> *ptr, rapidxml::xml_document<char> *doc)
		: CXmlNode(ptr), m_doc(doc) {}
	friend class XmlDocument;

	rapidxml::xml_document<char> *m_doc = nullptr;
};

class XmlDocument {
  public:
	static constexpr int default_max_file_size = 64 * 1024 * 1024;
	XmlDocument();
	XmlDocument(XmlDocument &&);
	~XmlDocument();
	XmlDocument &operator=(XmlDocument &&);

	static Ex<XmlDocument> load(ZStr file_name, int max_size = default_max_file_size);
	static Ex<XmlDocument> make(CSpan<char> xml_data);

	Ex<void> save(ZStr file_name) const;
	Ex<void> save(FileStream &) const;

	XmlNode addChild(Str name, Str value = {});

	// TODO: const should return CXmlNode
	XmlNode child(Str name = {}) const;

	Str own(Str);
	Str own(const TextFormatter &str) { return own(str.text()); }

	string lastNodeInfo() const;

  protected:
	Dynamic<rapidxml::xml_document<char>> m_ptr;
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
		template <class C> static auto testL(int) -> decltype(C::load(DECLVAL(CXmlNode)));
		template <class C> static auto testL(...) -> Empty;

		template <class C>
		static auto testS(int) -> decltype(DECLVAL(const C &).save(DECLVAL(XmlNode)));
		template <class C> static auto testS(...) -> Empty;

		static constexpr bool loadable =
								  is_one_of<decltype(testL<T>(0)), Ex<T>, T> && !is_same<T, bool>,
							  saveable = !is_same<decltype(testS<T>(0)), Empty>;

		template <class C> static auto testFP(int) -> decltype(load(DECLVAL(CXmlNode), Type<C>()));
		template <class C> static auto testFP(...) -> Empty;

		template <class C>
		static auto testFS(int) -> decltype(save(DECLVAL(XmlNode), DECLVAL(const C &)));
		template <class C> static auto testFS(...) -> Empty;

		static constexpr bool func_loadable = is_one_of<decltype(testFP<T>(0)), Ex<T>, T>,
							  func_saveable = !is_same<decltype(testFS<T>(0)), Empty>;
	};
}

// To make type xml_saveable, you have to satisfy one of these conditions:
// - provide save(XmlNode, const T&)
// - provide T::save(XmlNode) const
// - make sure that T is formattible
template <class T>
constexpr bool is_xml_saveable =
	detail::XmlTraits<T>::saveable || is_formattible<T> || detail::XmlTraits<T>::func_saveable;

// To make type xml_constructible, you have to satisfy one of these conditions:
// - provide load(CXmlNode, Type<T>) -> Expected<T> | T
// - provide function T::load(CXmlNode) -> Expected<T> | T
// - make sure that T is parsable
template <class T>
constexpr bool is_xml_loadable =
	detail::XmlTraits<T>::loadable || is_parsable<T> || detail::XmlTraits<T>::func_loadable;

// TODO: make sure that loading functions don't generate exceptions
template <class T, EnableIf<is_xml_loadable<T>>...> Ex<T> load(CXmlNode node) {
	if constexpr(detail::XmlTraits<T>::func_loadable)
		return load(node, Type<T>());
	else if constexpr(detail::XmlTraits<T>::loadable)
		return T::load(node);
	else
		return node.value<T>();
}

template <class T, EnableIf<detail::XmlTraits<T>::saveable || is_formattible<T>>...>
void save(XmlNode node, const T &value) {
	if constexpr(detail::XmlTraits<T>::saveable)
		value.save(node);
	else
		node.setValue(value);
}
}
