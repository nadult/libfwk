// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/type_info_gen.h"

#include <fwk/format.h>
#include <fwk/hash_map.h>

#ifndef FWK_PLATFORM_MSVC
#include <cxxabi.h>
#endif

/*
// Problems with __PRETTY_FUNCTION__:
// - different compilers give different results in __PRETTY_FUNCTION__
// - a bit longer compile times?
// - cxa_demangle should give similar results

// __PRETTY_FUNCTION__ based type names:
template <int N> struct FixedString {
	constexpr FixedString(char const *s) {
		for(int i = 0; i < N; i++)
			buf[i] = s[i];
		buf[N] = 0;
	}
	char buf[N + 1];
};

static constexpr const char *getName() {
	char const *name = __PRETTY_FUNCTION__;
	while(*name++ != '=')
		;
	while(*name == ' ')
		name++;
	return name;
}

static constexpr int getNameSize() {
	auto start = getName();
	auto end = start;
	int bracketCount = 1;
	for(;; ++end) {
		if(*end == '[')
			bracketCount++;
		if(*end == ']') {
			--bracketCount;
			if(!bracketCount)
				return end - start;
		}
	}
	return {};
}

#ifdef __clang__
	static constexpr FixedString<getNameSize()> name{getName()};
#endif
*/

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

	auto registerBasicTypes = []() {
		[[maybe_unused]] auto dummy = typeInfo<void>();
		return 0;
	}();

	void addTypeName(const TypeInfoData &data, const char *mangled_name) {
#ifdef FWK_PLATFORM_MSVC
		string demangled_name;
		int length = strlen(mangled_name);
		demangled_name.reserve(length);
		Pair<const char *, int> prefixes[] = {{"struct ", 7}, {"class ", 6}, {"enum ", 5}};

		for(int i = 0; i < length; i++) {
			bool prefix_matched = false;
			if(i == 0 || (!isalnum(mangled_name[i - 1]) && mangled_name[i - 1] != '_'))
				for(auto [prefix, prefix_len] : prefixes)
					if(strncmp(prefix, mangled_name + i, prefix_len) == 0) {
						i += prefix_len - 1;
						prefix_matched = true;
						break;
					}
			if(!prefix_matched)
				demangled_name += mangled_name[i];
		}
#else
		int status;
		auto demangled_name = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
		if(status != 0)
			FWK_FATAL("Error while demangling name: %s; cxa_demangle status: %d", mangled_name,
					  status);
#endif

		bool is_const = data.is_const;
		bool is_volatile = data.is_volatile;
		if(data.reference_base) {
			is_const = data.reference_base->is_const;
			is_volatile = data.reference_base->is_volatile;
		}

		string name = format("%%%%", demangled_name, is_const ? " const" : "",
							 is_volatile ? " volatile" : "", data.reference_base ? "&" : "");
#ifndef FWK_PLATFORM_MSVC
		free(demangled_name);
#endif

		auto &name_to_id = invTypeNames();
		auto id = (long long)&data;
		typeNames()[id] = name;
		if(name_to_id.find(name) != name_to_id.end())
			FWK_FATAL("Multiple entries found for: %s", name.c_str());
		name_to_id[name] = id;
	}
}

TypeInfo::TypeInfo() : m_data(&detail::TypeData<void>::data) {}

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

void TypeInfo::operator>>(TextFormatter &fmt) const { fmt << name(); }

ZStr TypeInfo::name() const {
	auto &map = detail::typeNames();
	auto it = map.find(id());
	DASSERT(it);
	return it->value.c_str();
}

const HashMap<string, TypeId> &TypeInfo::nameToId() { return detail::invTypeNames(); }
const HashMap<TypeId, string> &TypeInfo::idToName() { return detail::typeNames(); }

Maybe<TypeInfo> typeInfo(const char *type_name) {
	auto &map = detail::invTypeNames();
	if(auto it = map.find(type_name))
		return TypeInfo(it->value);
	return none;
}

int hash(TypeInfo info) { return hash(info.id()); }
}
