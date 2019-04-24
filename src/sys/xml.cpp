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

#include "fwk/filesystem.h"
#include "fwk/sys/file_stream.h"
#include "fwk/sys/on_fail.h"
#include "fwk/sys/xml.h"

#define assert DASSERT

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#undef assert

#include <csetjmp>
#include <cstdio>
#include <cstring>

using namespace rapidxml;

namespace {
struct XmlDebugHelper {
	xml_node<> *last_node = nullptr;
	xml_attribute<> *last_attrib = nullptr;
	const char *pstring = nullptr;
	std::jmp_buf jump_buffer = {};
	int pstring_len = 0;
};
__thread XmlDebugHelper t_xml_debug;

void touch(xml_node<> *ptr = nullptr, xml_attribute<> *attrib = nullptr) {
	auto &helper = t_xml_debug;
	helper.last_node = ptr;
	helper.last_attrib = attrib;
}

// Note: In case of adding remove node functionality
// untouch(xml_node<>) should be added as well
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

namespace fwk {

static const char *strOrNull(Str str) { return str ? str.data() : nullptr; }

ZStr CXmlNode::hasAttrib(Str name) const {
	PASSERT(name);
	xml_attribute<> *attrib = m_ptr->first_attribute(name.data(), name.size());
	touch(m_ptr, attrib);

	return attrib ? attrib->value() : ZStr();
}

ZStr CXmlNode::attrib(Str name) const {
	PASSERT(name);
	xml_attribute<> *attrib = m_ptr->first_attribute(name.data(), name.size());
	touch(m_ptr, attrib);

	if(!attrib || !attrib->value()) {
		EXCEPTION("attribute '%' not found in node: %\n", name, this->name());
		return "";
	}
	return attrib->value();
}

ZStr CXmlNode::attrib(Str name, ZStr default_value) const {
	ZStr value = hasAttrib(name);
	return value ? value : default_value;
}

ZStr CXmlNode::name() const { return {m_ptr->name(), (int)m_ptr->name_size()}; }
ZStr CXmlNode::value() const { return {m_ptr->value(), (int)m_ptr->value_size()}; }

CXmlNode CXmlNode::sibling(Str name) const {
	auto *sibling = m_ptr->next_sibling(strOrNull(name), name.size());
	touch(sibling ? sibling : m_ptr);
	return {sibling};
}

CXmlNode CXmlNode::child(Str name) const {
	auto *child_node = m_ptr->first_node(strOrNull(name), name.size());
	touch(child_node ? child_node : m_ptr);
	return {child_node};
}

XmlNode::XmlNode(CXmlNode cnode) : CXmlNode(cnode) {
	if(m_ptr)
		m_doc = m_ptr->document();
}

Str XmlNode::own(Str str) {
	char *ptr = m_doc->allocate_string(nullptr, str.size() + 1);
	memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;
	return ptr;
}

void XmlNode::addAttrib(Str name, int value) {
	char str_value[32];
	sprintf(str_value, "%d", value);
	addAttrib(name, own(str_value));
}

void XmlNode::addAttrib(Str name, Str value) {
	DASSERT(name && "Attrib names cannot be empty");
	auto *attrib =
		m_doc->allocate_attribute(name.data(), strOrNull(value), name.size(), value.size());
	m_ptr->append_attribute(attrib);
	touch(m_ptr, attrib);
}

bool XmlNode::validNodeName(Str name) {
	if(!name)
		return false;
	for(auto c : name)
		if(!isalnum(c) && c != '_')
			return false;
	return true;
}

XmlNode XmlNode::addChild(Str name, Str value) {
	DASSERT(validNodeName(name));
	xml_node<> *node = m_doc->allocate_node(node_element, name.data(), strOrNull(value),
											name.size(), value.size());
	m_ptr->append_node(node);
	touch(node);

	return XmlNode(node, m_doc);
}

void XmlNode::setValue(Str text) { m_ptr->value(text.data(), text.size()); }

XmlNode XmlNode::sibling(Str name) const {
	auto cnode = CXmlNode::sibling(name);
	return {cnode.m_ptr, m_doc};
}

XmlNode XmlNode::child(Str name) const {
	auto cnode = CXmlNode::child(name);
	return {cnode.m_ptr, m_doc};
}

XmlDocument::XmlDocument() : m_ptr(uniquePtr<xml_document<>>()) {}

XmlDocument::XmlDocument(XmlDocument &&) = default;
XmlDocument::~XmlDocument() {
	if(m_ptr)
		untouch(m_ptr.get());
}
XmlDocument &XmlDocument::operator=(XmlDocument &&) = default;

Str XmlDocument::own(Str str) {
	char *ptr = m_ptr->allocate_string(nullptr, str.size() + 1);
	memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;
	return ptr;
}

XmlNode XmlDocument::addChild(Str name, Str value) {
	DASSERT(XmlNode::validNodeName(name));
	xml_node<> *node = m_ptr->allocate_node(node_element, name.data(), strOrNull(value),
											name.size(), value.size());
	m_ptr->append_node(node);
	return XmlNode(node, m_ptr.get());
}

XmlNode XmlDocument::child(Str name) const {
	return XmlNode(m_ptr->first_node(strOrNull(name), name.size()), m_ptr.get());
}

Ex<XmlDocument> XmlDocument::load(ZStr file_name, int max_size) {
	auto data = loadFile(file_name, max_size);
	return data ? make(*data) : data.error();
}

Ex<void> XmlDocument::save(ZStr file_name) const {
	auto file = fileSaver(file_name);
	if(!file)
		return file.error();
	return save(*file);
}
}

// This may be called only when parsing document
void rapidxml::parse_error_handler(const char *what, void *where) {
	fwk::Str parsed_text(t_xml_debug.pstring, t_xml_debug.pstring_len);
	auto pos = parsed_text.utf8TextPos((const char *)where);
	fwk::TextFormatter fmt;
	fmt("XML parsing error: %", what);
	if(pos != fwk::Pair<int>())
		fmt(" at: line:% col:%", pos.first, pos.second);
	using namespace fwk;
	EXCEPTION("%", fmt.text());
	std::longjmp(t_xml_debug.jump_buffer, 1);
}

namespace fwk {

Ex<XmlDocument> XmlDocument::make(CSpan<char> data) {
	XmlDocument doc;

	char *xml_string = doc.m_ptr->allocate_string(0, data.size() + 1);
	copy(span(xml_string, data.size()), data);
	xml_string[data.size()] = 0;

	doc.m_xml_string = {xml_string, data.size()};
	t_xml_debug.pstring = xml_string;
	t_xml_debug.pstring_len = data.size();

	// TODO: verify that there are no leaks
	if(setjmp(t_xml_debug.jump_buffer)) {
		doc.m_ptr->release();
		t_xml_debug.pstring = nullptr;
		t_xml_debug.pstring_len = 0;
		return getMergedExceptions();
	} else {
		doc.m_ptr->parse<0>(xml_string);
	}

	t_xml_debug.pstring = nullptr;
	t_xml_debug.pstring_len = 0;

	return move(doc);
}

Ex<void> XmlDocument::save(FileStream &sr) const {
	vector<char> buffer;
	print(std::back_inserter(buffer), *m_ptr);
	sr.saveData(buffer);
	EXPECT_CATCH();
	return {};
}

string XmlDocument::lastNodeInfo() const {
	const auto &helper = t_xml_debug;
	TextFormatter out;

	if(helper.last_node && helper.last_node->document() == m_ptr.get()) {
		auto nname = helper.last_node->name();
		auto pos = m_xml_string.utf8TextPos(nname);
		out("Last XML node: '%'", nname);
		if(pos != Pair<int>())
			out(" at: line:% col:%", pos.first, pos.second);

		if(helper.last_attrib) {
			auto name = helper.last_attrib->name();
			auto pos = m_xml_string.utf8TextPos(name);
			out("\nLast XML attribute: '%'", name);
			if(pos != Pair<int>())
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
