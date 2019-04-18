// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/sys/on_fail.h"
#include "fwk/sys/resource_manager.h"
#include "fwk/sys/xml.h"

namespace fwk {

template <class T> class XmlLoader : public ResourceLoader<T> {
  public:
	XmlLoader(const string &prefix, const string &suffix, string node_name)
		: ResourceLoader<T>(prefix, suffix), m_node_name(move(node_name)) {}

	// TODO: remove immutable, handle errors
	// TODO: remove xmlloader ?
	immutable_ptr<T> operator()(const string &name) {
		auto doc = move(XmlDocument::load(ResourceLoader<T>::fileName(name)).get());
		ON_FAIL("While loading resource: %", name);

		XmlOnFailGuard guard(doc);
		XmlNode child = doc.child(m_node_name.empty() ? nullptr : m_node_name.c_str());
		if(!child) // TODO: return Expected<>
			FATAL("Cannot find node '%s' in XML document", m_node_name.c_str());

		auto obj = T::loadFromXML(child);
		if constexpr(is_same<decltype(obj), Expected<T>>) {
			return immutable_ptr<T>(move(obj.get()));
		} else {
			return immutable_ptr<T>(move(obj));
		}
	}

  protected:
	string m_node_name;
};
}
