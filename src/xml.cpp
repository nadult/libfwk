/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

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

// TODO: snprintf / snscanf everywhere?
void XMLNode::addAttrib(const char *name, float value) {
	int ivalue = (int)value;
	if(value == (float)ivalue) {
		addAttrib(name, ivalue);
		return;
	}

	char str_value[64];
	snprintf(str_value, sizeof(str_value), "%f", value);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, int value) {
	char str_value[32];
	sprintf(str_value, "%d", value);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const char *value) {
	m_ptr->append_attribute(m_doc->allocate_attribute(name, value));
}

void XMLNode::parsingError(const char *attrib_name) const {
	THROW("Error while parsing attribute value: %s in node: %s\n", attrib_name, name());
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

int XMLNode::intAttrib(const char *name) const {
	const char *str = attrib(name);
	int out;
	if(sscanf(str, "%d", &out) != 1)
		parsingError(name);
	return out;
}

float XMLNode::floatAttrib(const char *name) const {
	const char *str = attrib(name);
	float out;
	if(sscanf(str, "%f", &out) != 1)
		parsingError(name);
	return out;
}

int XMLNode::intAttrib(const char *name, int default_value) const {
	const char *attrib = hasAttrib(name);
	return attrib ? toInt(attrib) : default_value;
}

float XMLNode::floatAttrib(const char *name, int default_value) const {
	const char *attrib = hasAttrib(name);
	return attrib ? toFloat(attrib) : default_value;
}

const int2 XMLNode::int2Attrib(const char *name) const {
	const char *str = attrib(name);
	int2 out;
	if(sscanf(str, "%d %d", &out.x, &out.y) != 2)
		parsingError(name);
	return out;
}

const int3 XMLNode::int3Attrib(const char *name) const {
	const char *str = attrib(name);
	int3 out;
	if(sscanf(str, "%d %d %d", &out.x, &out.y, &out.z) != 3)
		parsingError(name);
	return out;
}

const float2 XMLNode::float2Attrib(const char *name) const {
	const char *str = attrib(name);
	float2 out;
	if(sscanf(str, "%f %f", &out.x, &out.y) != 2)
		parsingError(name);
	return out;
}

const float3 XMLNode::float3Attrib(const char *name) const {
	const char *str = attrib(name);
	float3 out;
	if(sscanf(str, "%f %f %f", &out.x, &out.y, &out.z) != 3)
		parsingError(name);
	return out;
}

void XMLNode::addAttrib(const char *name, const int2 &value) {
	char str_value[65];
	sprintf(str_value, "%d %d", value.x, value.y);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const int3 &value) {
	char str_value[98];
	sprintf(str_value, "%d %d %d", value.x, value.y, value.z);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const float2 &value) {
	int2 ivalue = (int2)value;
	if(value == (float2)ivalue) {
		addAttrib(name, ivalue);
		return;
	}

	char str_value[129];
	sprintf(str_value, "%f %f", value.x, value.y);
	addAttrib(name, own(str_value));
}

void XMLNode::addAttrib(const char *name, const float3 &value) {
	int3 ivalue = (int3)value;
	if(value == (float3)ivalue) {
		addAttrib(name, ivalue);
		return;
	}

	char str_value[194];
	sprintf(str_value, "%f %f %f", value.x, value.y, value.z);
	addAttrib(name, own(str_value));
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

XMLDocument::XMLDocument() : m_ptr(new xml_document<>) {}
XMLDocument::XMLDocument(XMLDocument &&other) : m_ptr(std::move(other.m_ptr)) {}
XMLDocument::~XMLDocument() {}

void XMLDocument::operator=(XMLDocument &&other) { m_ptr = std::move(other.m_ptr); }

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
		THROW("rapidxml exception caught: %s at: %d", ex.what(), ex.where<char>() - xml_string);
	}
}

void XMLDocument::save(Stream &sr) const {
	vector<char> buffer;
	print(std::back_inserter(buffer), *m_ptr);
	sr.saveData(&buffer[0], buffer.size());
}

bool toBool(const char *input) {
	if(strcasecmp(input, "true") == 0)
		return true;
	if(strcasecmp(input, "false") == 0)
		return false;
	return toInt(input) != 0;
}

static void toInt(const char *input, int count, int *out) {
	DASSERT(input);
	if(!input[0]) {
		for(int i = 0; i < count; i++)
			out[i] = 0;
		return;
	}
	
	const char *format = "%d %d %d %d";
	if(sscanf(input, format + (4 - count) * 3, out + 0, out + 1, out + 2, out + 3) != count)
		THROW("Error while converting string \"%s\" to int%d", input, count);
}

int toInt(const char *input) {
	int out;
	toInt(input, 1, &out);
	return out;
}

int2 toInt2(const char *input) {
	int2 out;
	toInt(input, 2, &out.x);
	return out;
}

int3 toInt3(const char *input) {
	int3 out;
	toInt(input, 3, &out.x);
	return out;
}

int4 toInt4(const char *input) {
	int4 out;
	toInt(input, 4, &out.x);
	return out;
}

static void toFloat(const char *input, int count, float *out) {
	DASSERT(input);
	if(!input[0]) {
		for(int i = 0; i < count; i++)
			out[i] = 0.0f;
		return;
	}

	const char *format = "%f %f %f %f";
	if(sscanf(input, format + (4 - count) * 3, out + 0, out + 1, out + 2, out + 3) != count)
		THROW("Error while converting string \"%s\" to float%d", input, count);
}

float toFloat(const char *input) {
	float out;
	toFloat(input, 1, &out);
	return out;
}

float2 toFloat2(const char *input) {
	float2 out;
	toFloat(input, 2, &out.x);
	return out;
}

float3 toFloat3(const char *input) {
	float3 out;
	toFloat(input, 3, &out.x);
	return out;
}

float4 toFloat4(const char *input) {
	float4 out;
	toFloat(input, 4, &out.x);
	return out;
}

const vector<string> toStrings(const char *input) {
	vector<string> out;
	const char *iptr = input;

	while(*iptr) {
		const char *next_space = strchr(iptr, ' ');
		int len = next_space ? next_space - iptr : strlen(iptr);

		out.emplace_back(string(iptr, iptr + len));

		if(!next_space)
			break;
		iptr = next_space + 1;
	}

	return out;
}

unsigned toFlags(const char *input, const char **strings, int num_strings, unsigned first_flag) {
	const char *iptr = input;

	unsigned out_value = 0;
	while(*iptr) {
		const char *next_space = strchr(iptr, ' ');
		int len = next_space ? next_space - iptr : strlen(iptr);

		bool found = false;
		for(int e = 0; e < num_strings; e++)
			if(strncmp(iptr, strings[e], len) == 0 && strings[e][len] == 0) {
				out_value |= first_flag << e;
				found = true;
				break;
			}

		if(!found) {
			char flags[1024], *ptr = flags;
			for(int i = 0; i < num_strings; i++)
				ptr += snprintf(ptr, sizeof(flags) - (ptr - flags), "%s ", strings[i]);
			if(num_strings)
				ptr[-1] = 0;

			THROW("Error while converting string \"%s\" to flags (%s)", input, flags);
		}

		if(!next_space)
			break;
		iptr = next_space + 1;
	}

	return out_value;
}
}
