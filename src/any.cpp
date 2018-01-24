// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/any.h"
#include "fwk/any_ref.h"

#include "fwk/filesystem.h"
#include "fwk/hash_map.h"
#include "fwk/sys/rollback.h"

namespace fwk {
namespace detail {

	using AnyTypeInfo = HashMap<TypeId, pair<AnyXmlConstructor, AnyXmlSaver>>;

	AnyTypeInfo &anyTypeInfos() {
		static AnyTypeInfo map(256);
		return map;
	}

	void registerAnyType(TypeInfo info, AnyXmlConstructor cfunc, AnyXmlSaver sfunc) {
		anyTypeInfos()[info.id()] = {cfunc, sfunc};
	}

	void reportAnyError(TypeInfo requested, TypeInfo current) {
		FATAL("Invalid value type in any: %s; requested: %s", current.name(), requested.name());
	}
}

using namespace detail;

Any::Any() = default;
Any::Any(Expected<Any> &&rhs) {
	if(rhs)
		*this = move(*rhs);
	else
		*this = move(rhs.error());
}
Any::Any(const Expected<Any> &rhs) {
	if(rhs)
		*this = *rhs;
	else
		*this = rhs.error();
}

FWK_COPYABLE_CLASS_IMPL(Any);
Any::Any(CXmlNode node, ZStr type_name) {
	if(!type_name.empty()) {
		auto &type_infos = anyTypeInfos();
		auto type_info = typeInfo(type_name.c_str());
		if(!type_info)
			CHECK_FAILED("No type-info for: '%s'", type_name.c_str());

		auto it = type_infos.find(type_info->id());
		if(it == type_infos.end())
			CHECK_FAILED("Type not found: %s", type_name.c_str());
		if(!it->second.first)
			CHECK_FAILED("Type '%s' is not XML-constructible", type_name.c_str());
		m_model = it->second.first(node);
		m_type = it->first;
	}
}

Any::Any(CXmlNode node) : Any(node, node.hasAttrib("_any_type")) {}

void Any::save(XmlNode node, bool save_type) const { AnyRef(*this).save(node, save_type); }
bool Any::xmlEnabled() const { return AnyRef(*this).xmlEnabled(); }

Any Any::tryConstruct(CXmlNode node) {
	return RollbackContext::begin([&] { return Any(node); });
}

Any Any::tryConstruct(CXmlNode node, ZStr type_name) {
	return RollbackContext::begin([&] { return Any(node, type_name); });
}
}
