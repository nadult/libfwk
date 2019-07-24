// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/any.h"
#include "fwk/any_ref.h"

#include "fwk/hash_map.h"
#include "fwk/sys/file_system.h"

namespace fwk {
namespace detail {

	using AnyTypeInfo = HashMap<TypeId, Pair<AnyXmlLoader, AnyXmlSaver>>;

	AnyTypeInfo &anyTypeInfos() {
		static AnyTypeInfo map(256);
		return map;
	}

	void registerAnyType(TypeInfo info, AnyXmlLoader cfunc, AnyXmlSaver sfunc) {
		anyTypeInfos()[info.id()] = {cfunc, sfunc};
	}

	void reportAnyError(TypeInfo requested, TypeInfo current) {
		FATAL("Invalid value type in any: %s; requested: %s", current.name(), requested.name());
	}
}

Any::Any() = default;
Any::Any(Ex<Any> &&rhs) {
	if(rhs)
		*this = move(*rhs);
	else
		*this = move(rhs.error());
}
Any::Any(const Ex<Any> &rhs) {
	if(rhs)
		*this = *rhs;
	else
		*this = rhs.error();
}

FWK_COPYABLE_CLASS_IMPL(Any);
Ex<Any> Any::load(CXmlNode node, ZStr type_name) {
	if(!type_name)
		return Any();
	auto &type_infos = detail::anyTypeInfos();
	auto type_info = typeInfo(type_name.c_str());
	if(!type_info)
		return ERROR("Type-info not found for: '%'", type_name);
	return load(node, *type_info);
}

Ex<Any> Any::load(CXmlNode node, TypeInfo type_info) {
	auto &type_infos = detail::anyTypeInfos();
	auto it = type_infos.find(type_info.id());
	if(it == type_infos.end())
		return ERROR("Any-type-info not found for: '%'", type_info.name());

	if(!it->second.first)
		return ERROR("Type '%' is not XML-constructible", type_info.name());

	Any out;
	out.m_model = EXPECT_PASS(it->second.first(node));
	out.m_type = it->first;
	return out;
}

Ex<Any> Any::load(CXmlNode node) { return load(node, node.hasAttrib("_any_type")); }

void Any::save(XmlNode node, bool save_type) const { AnyRef(*this).save(node, save_type); }
bool Any::xmlEnabled() const { return AnyRef(*this).xmlEnabled(); }

void Any::swap(Any &rhs) {
	fwk::swap(m_model, rhs.m_model);
	fwk::swap(m_type, rhs.m_type);
}
}
