// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/sys/resource_manager.h"
#include "fwk/sys/xml.h"

namespace fwk {

template <class T> class XmlLoader : public ResourceLoader<T> {
  public:
	XmlLoader(const string &prefix, const string &suffix, string node_name)
		: ResourceLoader<T>(prefix, suffix), m_node_name(move(node_name)) {}

	immutable_ptr<T> operator()(const string &name) {
		auto doc = move(XmlDocument::load(ResourceLoader<T>::fileName(name)).get());
		XmlOnFailGuard guard(doc);
		XmlNode child = doc.child(m_node_name.empty() ? nullptr : m_node_name.c_str());
		if(!child) // TODO: return Expected<>
			FATAL("Cannot find node '%s' in XML document", m_node_name.c_str());
		return immutable_ptr<T>(T::loadFromXML(child));
	}

  protected:
	string m_node_name;
};
}
