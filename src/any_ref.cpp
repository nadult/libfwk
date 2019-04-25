// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/any_ref.h"

#include "fwk/hash_map.h"
#include "fwk/sys/file_system.h"

namespace fwk {
namespace detail {
	using AnyTypeInfo = HashMap<TypeId, Pair<AnyXmlLoader, AnyXmlSaver>>;
	AnyTypeInfo &anyTypeInfos();
}

AnyRef::AnyRef(const Any &rhs)
	: m_ptr(rhs.m_model.get() ? rhs.m_model->ptr() : nullptr), m_type(rhs.m_type) {}

AnyRef::AnyRef() : m_ptr(nullptr) {}
AnyRef::AnyRef(const void *ptr, TypeInfo type_info) : m_ptr(ptr), m_type(type_info) {}

FWK_COPYABLE_CLASS_IMPL(AnyRef);

void AnyRef::save(XmlNode node, bool save_type) const {
	if(m_ptr) {
		auto &type_infos = detail::anyTypeInfos();
		auto it = type_infos.find(m_type.id());
		DASSERT(it != type_infos.end() && it->second.second && "Type is not XML-enabled");

		it->second.second(m_ptr, node);
		if(save_type)
			node.addAttrib("_any_type", m_type.name());
	}
}

bool AnyRef::xmlEnabled() const {
	auto &type_infos = detail::anyTypeInfos();
	auto it = type_infos.find(m_type.id());
	DASSERT(it != type_infos.end());
	return it->second.first && it->second.second;
}
}
