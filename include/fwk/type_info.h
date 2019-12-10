// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/str.h"

namespace fwk {

namespace detail {
	struct TypeInfoData {
		const TypeInfoData *const_or_not;
		const TypeInfoData *pointer_base;
		const TypeInfoData *reference_base;
		int size, alignment;
		bool is_const, is_volatile;
	};
}

using TypeId = long long;

struct TypeInfo {
	using Data = detail::TypeInfoData;
	TypeInfo(const Data &data) : m_data(&data) {}
	TypeInfo(TypeId id) : m_data(reinterpret_cast<const Data *>(id)) {}
	TypeInfo();

	TypeId id() const { return (long long)m_data; }
	bool isVolatile() const { return m_data->is_volatile; }
	bool isConst() const { return m_data->is_const; }
	bool isPointer() const { return m_data->pointer_base != nullptr; }
	bool isReference() const { return m_data->reference_base != nullptr; }
	int size() const { return m_data->size; }
	int alignment() const { return m_data->alignment; }

	Maybe<TypeInfo> pointerBase() const;
	Maybe<TypeInfo> referenceBase() const;
	TypeInfo asConst() const;
	TypeInfo asNotConst() const;

	// Implemented in type_info_gen.h
	template <class T> bool is() const;
	template <class... Types> bool isOneOf() const;

	ZStr name() const;

	bool operator==(const TypeInfo &rhs) const { return m_data == rhs.m_data; }
	bool operator<(const TypeInfo &rhs) const { return m_data < rhs.m_data; }

	explicit TypeInfo(Intrusive::EmptyMaybe) : m_data(nullptr) {}
	bool operator==(Intrusive::EmptyMaybe) const { return !m_data; }

	static const HashMap<string, TypeId> &nameToId();
	static const HashMap<TypeId, string> &idToName();

	void operator>>(fwk::TextFormatter &) const;

  private:
	const Data *m_data;
};

int hash(TypeInfo);
Maybe<TypeInfo> typeInfo(const char *type_name);
}
