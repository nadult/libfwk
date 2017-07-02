// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define RAPIDXML_NO_STREAMS
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "fwk_xml.h"
#include <sstream>
#include <cstring>
#include <cstdio>

using namespace rapidxml;

typedef xml_attribute<> XMLAttrib;

namespace fwk {

const char *XMLNode::own(const char *str) { return m_doc->allocate_string(str); }

void XMLNode::addAttrib(const char *name, int value) {
	char str_value[32];
	sprintf(str_value, "%d", value);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const char *value) {
	m_ptr->append_attribute(m_doc->allocate_attribute(name, value));
}

void XMLNode::parsingError(const char *attrib_name, const char *error_message) const {
	if(attrib_name)
		THROW("Error while parsing attribute value: %s in node: %s\n%s", attrib_name, name(),
			  error_message);
	else
		THROW("Error while parsing value in node: %s\n%s", name(), error_message);
}

const char *XMLNode::hasAttrib(const char *name) const {
	XMLAttrib *attrib = m_ptr->first_attribute(name);
	return attrib ? attrib->value() : nullptr;
}

const char *XMLNode::attrib(const char *name) const {
	XMLAttrib *attrib = m_ptr->first_attribute(name);
	if(!attrib || !attrib->value())
		THROW("attribute not found: %s in node: %s\n", name, this->name());
	return attrib->value();
}

const char *XMLNode::attrib(const char *name, const char *default_value) const {
	const char *value = hasAttrib(name);
	return value ? value : default_value;
}

const char *XMLNode::name() const { return m_ptr->name(); }

const char *XMLNode::value() const { return m_ptr->value(); }

XMLNode XMLNode::addChild(const char *name, const char *value) {
	xml_node<> *node = m_doc->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	return XMLNode(node, m_doc);
}

XMLNode XMLNode::sibling(const char *name) const {
	return XMLNode(m_ptr->next_sibling(name), m_doc);
}

XMLNode XMLNode::child(const char *name) const { return XMLNode(m_ptr->first_node(name), m_doc); }

XMLDocument::XMLDocument() : m_ptr(make_unique<xml_document<>>()) {}
XMLDocument::XMLDocument(Stream &stream) :XMLDocument() {
	stream >> *this;
}

XMLDocument::XMLDocument(XMLDocument &&) = default;
XMLDocument::~XMLDocument() = default;
XMLDocument &XMLDocument::operator=(XMLDocument &&) = default;

const char *XMLDocument::own(const char *str) { return m_ptr->allocate_string(str); }

XMLNode XMLDocument::addChild(const char *name, const char *value) const {
	xml_node<> *node = m_ptr->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	return XMLNode(node, m_ptr.get());
}

XMLNode XMLDocument::child(const char *name) const {
	return XMLNode(m_ptr->first_node(name), m_ptr.get());
}

void XMLDocument::load(const char *file_name) {
	DASSERT(file_name);
	Loader ldr(file_name);
	ldr >> *this;
}

void XMLDocument::save(const char *file_name) const {
	DASSERT(file_name);
	Saver svr(file_name);
	svr << *this;
}

void XMLDocument::load(Stream &sr) {
	m_ptr->clear();

	char *xml_string = m_ptr->allocate_string(0, sr.size() + 1);
	sr.loadData(xml_string, sr.size());
	xml_string[sr.size()] = 0;

	try {
		m_ptr->parse<0>(xml_string);
	} catch(const parse_error &ex) {
		THROW("rapidxml exception caught: %s at: %d", ex.what(),
			  (int)(size_t)(ex.where<char>() - xml_string));
	}
}

void XMLDocument::save(Stream &sr) const {
	vector<char> buffer;
	print(std::back_inserter(buffer), *m_ptr);
	sr.saveData(&buffer[0], buffer.size());
}
}
