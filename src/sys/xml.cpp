// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define RAPIDXML_NO_STREAMS
#define RAPIDXML_NO_STDLIB
#define RAPIDXML_NO_EXCEPTIONS

#include <cstdlib>
#include <new>

#ifdef assert
#undef assert
#endif

#define assert DASSERT

#include "fwk/sys/on_fail.h"
#include "fwk/sys/stream.h"
#include "fwk/sys/xml.h"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include <cstdio>
#include <cstring>

using namespace rapidxml;

namespace {
struct XmlDebugHelper {
	xml_node<> *last_node = nullptr;
	xml_attribute<> *last_attrib = nullptr;
	const char *pstring = nullptr;
	int pstring_len = 0;
};
__thread XmlDebugHelper t_xml_debug;

void touch(xml_node<> *ptr = nullptr, xml_attribute<> *attrib = nullptr) {
	auto &helper = t_xml_debug;
	helper.last_node = ptr;
	helper.last_attrib = attrib;
}

void untouch(xml_document<> *ptr) {
	if(ptr) {
		auto &helper = t_xml_debug;
		if(helper.last_node && helper.last_node->document() == ptr) {
			helper.last_node = nullptr;
			helper.last_attrib = nullptr;
		}
	}
}
}
namespace rapidxml {
void parse_error_handler(const char *what, void *where) {
	fwk::Str parsed_text(t_xml_debug.pstring, t_xml_debug.pstring_len);
	auto pos = parsed_text.utf8TextPos((const char *)where);
	fwk::TextFormatter fmt;
	fmt("XML parsing error: %", what);
	if(pos != fwk::pair<int, int>())
		fmt(" at: line:% col:%", pos.first, pos.second);
	CHECK_FAILED("%s", fmt.c_str());
}
}

namespace fwk {

const char *CXmlNode::hasAttrib(const char *name) const {
	xml_attribute<> *attrib = m_ptr->first_attribute(name);
	touch(m_ptr, attrib);

	return attrib ? attrib->value() : nullptr;
}

const char *CXmlNode::attrib(const char *name) const {
	xml_attribute<> *attrib = m_ptr->first_attribute(name);
	touch(m_ptr, attrib);

	if(!attrib || !attrib->value())
		CHECK_FAILED("attribute not found: %s in node: %s\n", name, this->name());
	return attrib->value();
}

const char *CXmlNode::attrib(const char *name, const char *default_value) const {
	const char *value = hasAttrib(name);
	return value ? value : default_value;
}

const char *CXmlNode::name() const { return m_ptr->name(); }
const char *CXmlNode::value() const { return m_ptr->value(); }

CXmlNode CXmlNode::sibling(const char *name) const {
	auto *sibling = m_ptr->next_sibling(name);
	touch(sibling ? sibling : m_ptr);
	return {sibling};
}

CXmlNode CXmlNode::child(const char *name) const {
	auto *child_node = m_ptr->first_node(name);
	touch(child_node ? child_node : m_ptr);
	return {child_node};
}

XmlNode::XmlNode(CXmlNode cnode) : CXmlNode(cnode) {
	if(m_ptr)
		m_doc = m_ptr->document();
}

const char *XmlNode::own(Str str) {
	char *ptr = m_doc->allocate_string(nullptr, str.size() + 1);
	memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;
	return ptr;
}

void XmlNode::addAttrib(const char *name, int value) {
	char str_value[32];
	sprintf(str_value, "%d", value);
	addAttrib(name, own(str_value));
}

void XmlNode::addAttrib(const char *name, Str value) {
	// FIXME: check it
	auto *attrib = m_doc->allocate_attribute(name, value.data(), 0, value.size());
	m_ptr->append_attribute(attrib);
	touch(m_ptr, attrib);
}

void XmlNode::addAttrib(const char *name, const char *value) {
	auto *attrib = m_doc->allocate_attribute(name, value);
	m_ptr->append_attribute(attrib);
	touch(m_ptr, attrib);
}

static void checkNodeName(ZStr name) {
	for(auto c : name)
		if(!isalnum(c) && c != '_')
			CHECK_FAILED("Invalid Xml node name: '%s' invalid char: '%c'",
						 name.limitSizeBack(40).c_str(), c);
}

XmlNode XmlNode::addChild(const char *name, const char *value) {
	checkNodeName(name);
	xml_node<> *node = m_doc->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	touch(node);

	return XmlNode(node, m_doc);
}

void XmlNode::setValue(const char *text) { m_ptr->value(text); }

XmlNode XmlNode::sibling(const char *name) const {
	auto cnode = CXmlNode::sibling(name);
	return {cnode.m_ptr, m_doc};
}

XmlNode XmlNode::child(const char *name) const {
	auto cnode = CXmlNode::child(name);
	return {cnode.m_ptr, m_doc};
}

XmlDocument::XmlDocument() : m_ptr(make_unique<xml_document<>>()) {}
XmlDocument::XmlDocument(ZStr file_name) : XmlDocument() {
	Loader loader(file_name);
	loader >> *this;
}
XmlDocument::XmlDocument(Stream &stream) : XmlDocument() { stream << *this; }

XmlDocument::XmlDocument(XmlDocument &&) = default;
XmlDocument::~XmlDocument() { untouch(m_ptr.get()); }
XmlDocument &XmlDocument::operator=(XmlDocument &&) = default;

const char *XmlDocument::own(Str str) {
	char *ptr = m_ptr->allocate_string(nullptr, str.size() + 1);
	memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;
	return ptr;
}

XmlNode XmlDocument::addChild(const char *name, const char *value) {
	checkNodeName(name);
	xml_node<> *node = m_ptr->allocate_node(node_element, name, value);
	m_ptr->append_node(node);
	return XmlNode(node, m_ptr.get());
}

XmlNode XmlDocument::child(const char *name) const {
	return XmlNode(m_ptr->first_node(name), m_ptr.get());
}

void XmlDocument::load(const char *file_name) {
	DASSERT(file_name);
	Loader ldr(file_name);
	ldr >> *this;
}

void XmlDocument::save(const char *file_name) const {
	DASSERT(file_name);
	Saver svr(file_name);
	svr << *this;
}

void XmlDocument::load(Stream &sr) {
	untouch(m_ptr.get());
	m_ptr->clear();

	int size = sr.size();
	char *xml_string = m_ptr->allocate_string(0, size + 1);
	sr.loadData(xml_string, size);
	xml_string[size] = 0;

	m_xml_string = {xml_string, size};
	t_xml_debug.pstring = xml_string;
	t_xml_debug.pstring_len = size;
	m_ptr->parse<0>(xml_string);
	t_xml_debug.pstring = nullptr;
	t_xml_debug.pstring_len = 0;
}

void XmlDocument::save(Stream &sr) const {
	vector<char> buffer;
	print(std::back_inserter(buffer), *m_ptr);
	sr.saveData(&buffer[0], buffer.size());
}

string XmlDocument::lastNodeInfo() const {
	const auto &helper = t_xml_debug;
	TextFormatter out;

	if(helper.last_node && helper.last_node->document() == m_ptr.get()) {
		auto nname = helper.last_node->name();
		auto pos = m_xml_string.utf8TextPos(nname);
		out("Last XML node: '%'", nname);
		if(pos != pair<int, int>())
			out(" at: line:% col:%", pos.first, pos.second);

		if(helper.last_attrib) {
			auto name = helper.last_attrib->name();
			auto pos = m_xml_string.utf8TextPos(name);
			out("\nLast XML attribute: '%'", name);
			if(pos != pair<int, int>())
				out(" at: line:% col:%", pos.first, pos.second);
		}
	}

	return out.text();
}

static ErrorChunk xmlOnFail(const void *doc) {
	return {reinterpret_cast<const XmlDocument *>(doc)->lastNodeInfo()};
}

XmlOnFailGuard::XmlOnFailGuard(const XmlDocument &doc) : m_document(doc) {
	onFailPush({xmlOnFail, &m_document});
}
XmlOnFailGuard::~XmlOnFailGuard() { onFailPop(); }
}
