/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_XML_H
#define FWK_XML_H

#include "fwk.h"
#include "fwk_math.h"

#ifndef RAPIDXML_HPP_INCLUDED

namespace rapidxml {
template <class Ch = char> class xml_node;
template <class Ch = char> class xml_document;
};

#endif

namespace fwk {

class XMLNode {
  public:
	XMLNode(const XMLNode &rhs) : m_ptr(rhs.m_ptr), m_doc(rhs.m_doc) {}
	XMLNode() : m_ptr(nullptr), m_doc(nullptr) {}

	// TODO: add function hasAttrib, use it in sys::config

	// TODO: change to tryAttrib
	// Returns nullptr if not found
	const char *hasAttrib(const char *name) const;

	// Returns default value if attribute is not found
	int intAttrib(const char *name, int default_value) const;
	float floatAttrib(const char *name, int default_value) const;

	// If an attribute cannot be found or properly parsed,
	// then exception is thrown
	const char *attrib(const char *name) const;
	float floatAttrib(const char *name) const;
	int intAttrib(const char *name) const;

	const int2 int2Attrib(const char *name) const;
	const int3 int3Attrib(const char *name) const;

	const float2 float2Attrib(const char *name) const;
	const float3 float3Attrib(const char *name) const;

	// TODO: make toInt, toFloat2 functions so that they can be used not only on attribs

	// When adding new nodes, you have to make sure that strings given as
	// arguments will exist as long as XMLNode exists; use 'own' method
	// to reallocate them in the memory pool if you're unsure
	void addAttrib(const char *name, const char *value);
	void addAttrib(const char *name, float value);
	void addAttrib(const char *name, int value);

	void addAttrib(const char *name, const int2 &value);
	void addAttrib(const char *name, const int3 &value);
	void addAttrib(const char *name, const float2 &value);
	void addAttrib(const char *name, const float3 &value);

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
	void parsingError(const char *) const;
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

// These functions expect valid strings and throw on error

bool toBool(const char *input);
int toInt(const char *input);

float toFloat(const char *input);
float2 toFloat2(const char *input);
float3 toFloat3(const char *input);
float4 toFloat4(const char *input);

const vector<string> toStrings(const char *input);
unsigned toFlags(const char *input, const char **flags, int num_flags, unsigned first_flag);

}

#endif
