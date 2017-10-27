// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_XML_H
#define FWK_XML_H

#include "fwk/format.h"
#include "fwk/sys_base.h"
#include "fwk_parse.h"

#ifndef RAPIDXML_HPP_INCLUDED

namespace rapidxml {
template <class Ch> class xml_node;
template <class Ch> class xml_document;
};

#endif

namespace fwk {

class XMLNode {
  public:
	XMLNode(const XMLNode &) = default;
	XMLNode() = default;

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

	// When adding new nodes, you have to make sure that strings given as
	// arguments will exist as long as XMLNode exists; use 'own' method
	// to reallocate them in the memory pool if you're not sure
	void addAttrib(const char *name, const char *value);
	void addAttrib(const char *name, int value);

	template <class T> void addAttrib(const char *name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		addAttrib(name, own(formatter));
	}

	XMLNode addChild(const char *name, const char *value = nullptr);
	XMLNode sibling(const char *name = nullptr) const;
	XMLNode child(const char *name = nullptr) const;

	template <class T> XMLNode addChild(const char *name, const T &value) {
		TextFormatter formatter(256, {FormatMode::plain});
		formatter << value;
		return addChild(name, own(formatter));
	}

	void next() { *this = sibling(name()); }

	const char *value() const;

	template <class T> T value() const { return fromString<T>(value()); }
	template <class T> T value(T default_value) const {
		const char *val = value();
		return val[0] ? value<T>() : default_value;
	}
	template <class T> T childValue(const char *child_name, T default_value) const {
		XMLNode child_node = child(child_name);
		const char *val = child_node ? child_node.value() : "";
		return val[0] ? child_node.value<T>() : default_value;
	}

	const char *name() const;

	const char *own(const char *str);
	const char *own(const string &str) { return own(str.c_str()); }
	const char *own(const TextFormatter &str) { return own(str.text()); }
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	XMLNode(rapidxml::xml_node<char> *ptr, rapidxml::xml_document<char> *doc)
		: m_ptr(ptr), m_doc(doc) {}
	friend class XMLDocument;

	rapidxml::xml_node<char> *m_ptr = nullptr;
	rapidxml::xml_document<char> *m_doc = nullptr;
};

class XMLDocument {
  public:
	XMLDocument();
	XMLDocument(Stream &);
	XMLDocument(XMLDocument &&);
	~XMLDocument();
	XMLDocument &operator=(XMLDocument &&);

	void load(const char *file_name);
	void save(const char *file_name) const;

	void load(Stream &);
	void save(Stream &) const;

	XMLNode addChild(const char *name, const char *value = nullptr) const;
	XMLNode child(const char *name = nullptr) const;
	XMLNode child(const string &name) const { return child(name.c_str()); }

	const char *own(const char *str);
	const char *own(const string &str) { return own(str.c_str()); }
	const char *own(const TextFormatter &str) { return own(str.text()); }

	string lastNodeInfo() const;

  protected:
	unique_ptr<rapidxml::xml_document<char>> m_ptr;
	CString m_xml_string;
};

// Use it to get information about currently parsed XML document on error
class XMLOnFailGuard {
  public:
	XMLOnFailGuard(const XMLDocument &);
	~XMLOnFailGuard();

	const XMLDocument &m_document;
};
}

#endif
