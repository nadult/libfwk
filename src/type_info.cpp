// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/type_info_gen.h"

#include <cxxabi.h>
#include <fwk/format.h>
#include <fwk/hash_map.h>

namespace fwk {
namespace detail {

	auto &typeNames() {
		static HashMap<TypeId, string> map(256);
		return map;
	}

	auto &invTypeNames() {
		static HashMap<string, TypeId> map(256);
		return map;
	}

	void addTypeName(const TypeInfoData &data, const char *mangled_name) {
		int status;
		auto demangled_name = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
		if(status != 0)
			FATAL("Error while demangling name: %s; cxa_demangle status: %d", mangled_name, status);

		bool is_const = data.is_const;
		bool is_volatile = data.is_volatile;
		if(data.reference_base) {
			is_const = data.reference_base->is_const;
			is_volatile = data.reference_base->is_volatile;
		}

		string name = format("%%%%", demangled_name, is_const ? " const" : "",
							 is_volatile ? " volatile" : "", data.reference_base ? "&" : "");
		free(demangled_name);

		auto &name_to_id = invTypeNames();
		auto id = (long long)&data;
		typeNames()[id] = name;
		if(name_to_id.find(name) != name_to_id.end())
			FATAL("Multiple entries found for: %s", name.c_str());
		name_to_id[name] = id;
	}
}

TypeInfo::TypeInfo() : m_data(&detail::TypeData<void>::data) {}

const char *TypeInfo::name() const { return detail::typeNames()[id()].c_str(); }

TypeInfo TypeInfo::asConst() const {
	if(m_data->is_const || !m_data->const_or_not)
		return *this;
	return TypeInfo(*m_data->const_or_not);
}
TypeInfo TypeInfo::asNotConst() const {
	if(!m_data->is_const || !m_data->const_or_not)
		return *this;
	return TypeInfo(*m_data->const_or_not);
}

Maybe<TypeInfo> TypeInfo::pointerBase() const {
	return m_data->pointer_base ? TypeInfo(*m_data->pointer_base) : Maybe<TypeInfo>();
}
Maybe<TypeInfo> TypeInfo::referenceBase() const {
	return m_data->reference_base ? TypeInfo(*m_data->reference_base) : Maybe<TypeInfo>();
}

fwk::TextFormatter &operator<<(fwk::TextFormatter &fmt, const TypeInfo &type_info) {
	return (fmt << type_info.name());
}

const HashMap<string, TypeId> &TypeInfo::nameToId() { return detail::invTypeNames(); }
const HashMap<TypeId, string> &TypeInfo::idToName() { return detail::typeNames(); }

Maybe<TypeInfo> typeInfo(const char *type_name) {
	auto &map = detail::invTypeNames();
	auto it = map.find(type_name);
	if(it != map.end())
		return TypeInfo(it->second);
	return none;
}

int hash(TypeInfo info) { return hash(info.id()); }
}
