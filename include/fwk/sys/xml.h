// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/parse.h"
#include "fwk/sys_base.h"
#include "fwk/unique_ptr.h"

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

	template <class T, class RT = std::decay_t<T>> RT attrib(const char *name, T &&or_else) const {
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
	XmlDocument(ZStr file_name);
	XmlDocument(Stream &);
	XmlDocument(XmlDocument &&);
	~XmlDocument();
	XmlDocument &operator=(XmlDocument &&);

	void load(const char *file_name);
	void save(const char *file_name) const;

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
}
