/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_XML_H
#define FWK_XML_H

#include "fwk_base.h"
#include "fwk_math.h"

#ifndef RAPIDXML_HPP_INCLUDED

namespace rapidxml {
template <class Ch = char> class xml_node;
template <class Ch = char> class xml_document;
};

#endif

namespace fwk {

namespace xml_conversions {

	template <class T> T fromString(const char *str) {
		THROW("xml_conversions::fromString unimplemented for given type");
	}

	template <> bool fromString<bool>(const char *);
	template <> int fromString<int>(const char *);
	template <> int2 fromString<int2>(const char *);
	template <> int3 fromString<int3>(const char *);
	template <> int4 fromString<int4>(const char *);
	template <> float fromString<float>(const char *);
	template <> float2 fromString<float2>(const char *);
	template <> float3 fromString<float3>(const char *);
	template <> float4 fromString<float4>(const char *);
	template <> FRect fromString<FRect>(const char *);
	template <> IRect fromString<IRect>(const char *);
	template <> vector<string> fromString<vector<string>>(const char *);

	template <class T> void toString(const T &value, TextFormatter &out) {
		THROW("xml_conversions::toString unimplemented for given type");
	}

	template <> void toString(const bool &value, TextFormatter &out);
	template <> void toString(const int &value, TextFormatter &out);
	template <> void toString(const int2 &value, TextFormatter &out);
	template <> void toString(const int3 &value, TextFormatter &out);
	template <> void toString(const int4 &value, TextFormatter &out);
	template <> void toString(const float &value, TextFormatter &out);
	template <> void toString(const float2 &value, TextFormatter &out);
	template <> void toString(const float3 &value, TextFormatter &out);
	template <> void toString(const float4 &value, TextFormatter &out);
	template <> void toString(const FRect &value, TextFormatter &out);
	template <> void toString(const IRect &value, TextFormatter &out);
}

class XMLNode {
  public:
	XMLNode(const XMLNode &rhs) : m_ptr(rhs.m_ptr), m_doc(rhs.m_doc) {}
	XMLNode() : m_ptr(nullptr), m_doc(nullptr) {}

	// TODO: add function hasAttrib, use it in sys::config

	// TODO: change to tryAttrib
	// Returns nullptr if not found
	const char *hasAttrib(const char *name) const;

	// If an attribute cannot be found or properly parsed then exception is thrown
	const char *attrib(const char *name) const;
	// If an attribute cannot be found then default_value will be returned
	const char *attrib(const char *name, const char *default_value) const;

	template <class T> T attrib(const char *name) const {
		try {
			return xml_conversions::fromString<T>(attrib(name));
		}
		catch(const Exception &ex) {
			parsingError(name, ex.what());
			return T();
		}
	}

	template <class T> T attrib(const char *name, T default_value) const {
		const char *value = hasAttrib(name);
		return value ? attrib<T>(value) : default_value;
	}

	// When adding new nodes, you have to make sure that strings given as
	// arguments will exist as long as XMLNode exists; use 'own' method
	// to reallocate them in the memory pool if you're unsure
	void addAttrib(const char *name, const char *value);
	void addAttrib(const char *name, float value);
	void addAttrib(const char *name, int value);

	template <class T> void addAttrib(const char *name, const T &value) {
		TextFormatter formatter;
		xml_conversions::toString(value, formatter);
		addAttrib(name, own(formatter.text()));
	}

	XMLNode addChild(const char *name, const char *value = nullptr);
	XMLNode sibling(const char *name = nullptr) const;
	XMLNode child(const char *name = nullptr) const;

	const char *value() const;
	const char *name() const;

	const char *own(const char *str);
	const char *own(const string &str) { return own(str.c_str()); }
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	XMLNode(rapidxml::xml_node<> *ptr, rapidxml::xml_document<> *doc) : m_ptr(ptr), m_doc(doc) {}
	void parsingError(const char *attrib_name, const char *error_message) const;
	friend class XMLDocument;

	rapidxml::xml_node<> *m_ptr;
	rapidxml::xml_document<> *m_doc;
};

class XMLDocument {
  public:
	XMLDocument();
	XMLDocument(const XMLDocument &) = delete;
	XMLDocument(XMLDocument &&);
	~XMLDocument();
	void operator=(XMLDocument &&);
	void operator=(const XMLDocument &) = delete;

	void load(const char *file_name);
	void save(const char *file_name) const;

	void load(Stream &);
	void save(Stream &) const;

	XMLNode addChild(const char *name, const char *value = nullptr) const;
	XMLNode child(const char *name = nullptr) const;

	const char *own(const char *str);

  protected:
	unique_ptr<rapidxml::xml_document<>> m_ptr;
};
}

#endif
