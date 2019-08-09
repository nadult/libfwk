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

// Allows easy access & modification of attributes
template <class Node, class T = Empty> struct XmlAccessor {
	static constexpr bool is_mutable = is_same<Node, XmlNode>;
	static constexpr bool has_default = !is_same<T, Empty>;

	XmlAccessor(Str name, Node node, T def = {}) : name(name), node(node), default_value(def) {}
	XmlAccessor(const XmlAccessor &) = delete;
	XmlAccessor(XmlAccessor &&) = delete;

	// Can assign any formattible value to it
	template <class U,
			  EnableIf<is_formattible<U> && (is_same<T, U> || !has_default) && is_mutable>...>
	void operator=(const U &val) {
		if constexpr(has_default)
			node.addAttrib(name, val, default_value);
		else
			node.addAttrib(name, val);
	}

	// Converts to parsable value
	template <class U, EnableIf<is_parsable<U> && (is_same<T, U> || !has_default)>...>
	operator U() const INST_EXCEPT {
		if constexpr(has_default)
			return node.attrib(name, default_value);
		else
			return node.template attrib<U>(name);
	}

	Str name;
	Node node;
	T default_value = {};
};

// -------------------------------------------------------------------------------------------
// ---  Immutable CXmlNode   -----------------------------------------------------------------
//
// Attribute & value functions raise exception on error, unless prefixed with try.
//
// Example use:
//   if(auto cnode = node.child("sub_node"))
//     float3 my_value = cnode("my_attribute");
//   if(auto maybe_val = cnode.maybeAttrib<float>("optional_attrib"))
//     float val = *maybe_val;
//   auto val = node("optional_attr2", 42 /*default value*/);

class CXmlNode {
  public:
	CXmlNode(const CXmlNode &) = default;
	CXmlNode() = default;

	ZStr attrib(Str name) const EXCEPT;
	template <class T = Empty>
	XmlAccessor<CXmlNode, T> operator()(Str name, const T &default_value = Empty()) const EXCEPT {
		return {name, *this, default_value};
	}

	ZStr tryAttrib(Str name, ZStr on_error = {}) const;
	ZStr tryAttrib(Str name, const char *on_error) const { return tryAttrib(name, ZStr(on_error)); }
	bool hasAttrib(Str name) const;

	template <class T> T attrib(Str name) const EXCEPT { return fromString<T>(attrib(name)); }
	template <class T> T attrib(Str name, const T &on_empty) const EXCEPT {
		ZStr value = tryAttrib(name);
		return value ? fromString<T>(value) : on_empty;
	}

	template <class T> T tryAttrib(Str name, const T &on_error = {}) const {
		ZStr val = tryAttrib(name);
		return val ? tryFromString<T>(val, on_error) : on_error;
	}
	template <class T> Maybe<T> maybeAttrib(Str name) const {
		ZStr val = tryAttrib(name);
		return val ? maybeFromString<T>(val) : Maybe<T>();
	}

	ZStr value() const;

	template <class T> T value() const EXCEPT { return fromString<T>(value()); }
	template <class T> T value(const T &on_empty) const EXCEPT {
		ZStr val = value();
		return val ? fromString<T>(val) : on_empty;
	}
	template <class T> T tryValue(const T &on_error = {}) const {
		ZStr val = value();
		return val ? tryFromString<T>(val, on_error) : on_error;
	}

	template <class T> T childValue(Str child_name, const T &on_empty) const EXCEPT {
		CXmlNode child_node = child(child_name);
		ZStr val = child_node ? child_node.value() : ZStr();
		return val ? child_node.value<T>() : on_empty;
	}
	template <class T> T tryChildValue(Str child_name, const T &on_error = {}) const {
		CXmlNode child_node = child(child_name);
		return child_node ? child_node.tryValue(on_error) : on_error;
	}

	CXmlNode sibling(Str name = {}) const;
	CXmlNode child(Str name = {}) const;

	ZStr name() const;

	explicit operator bool() const { return m_ptr != nullptr; }

	void next() { *this = sibling(name()); }

  private:
	CXmlNode(rapidxml::xml_node<char> *ptr) : m_ptr(ptr) {}
	friend class XmlDocument;
	friend class XmlNode;

	rapidxml::xml_node<char> *m_ptr = nullptr;
};

// -------------------------------------------------------------------------------------------
// ---  Mutable XMLNode   --------------------------------------------------------------------
//
// When adding new nodes, attributes or values you have to make sure
// that strings given as arguments will exist as long as XmlNode exists.
// Use own(...) method to reallocate them in XMLDocument's memory pool if
// you're not sure.

class XmlNode : public CXmlNode {
  public:
	explicit XmlNode(CXmlNode);
	XmlNode(const XmlNode &) = default;
	XmlNode() = default;

	template <class T = Empty>
	XmlAccessor<XmlNode, T> operator()(Str name, const T &default_value = Empty()) {
		return {name, *this, default_value};
	}

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

  private:
	XmlNode(rapidxml::xml_node<char> *ptr, rapidxml::xml_document<char> *doc)
		: CXmlNode(ptr), m_doc(doc) {}
	friend class XmlDocument;

	rapidxml::xml_document<char> *m_doc = nullptr;
};

// -------------------------------------------------------------------------------------------
// ---  XMLDocument   ------------------------------------------------------------------------

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

  private:
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

// -------------------------------------------------------------------------------------------
// ---  Type traits   ------------------------------------------------------------------------

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
