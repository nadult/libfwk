// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/light_tuple.h"
#include "fwk/maybe.h"
#include "fwk/sys_base.h"

namespace fwk {

// This can be overloaded for your own tags
template <class Tag1, class Tag2> constexpr bool isTagConvertible(Tag1 a, Tag2 b) { return false; }

template <auto tag, class Base = unsigned> class TagId;

// Type-safe tagged identifier
//
// Can be uninitialized with no_init, but the only thing that can be done with such
// value is assignment (it's verified in paranoid mode). Special care has to be taken
// when using those values.
template <class Tag, class Base_ = unsigned, unsigned base_bits = sizeof(Base_) * 8>
class GenericTagId {
  public:
	using Base = Base_;
	static_assert(sizeof(Base) <= 4);
	static_assert(base_bits >= 8 && base_bits <= 32);

	static constexpr int invalid_index = min<long long>(0x7fffffff - 1, (1ull << base_bits) - 2);
	static constexpr int uninitialized_index = invalid_index + 1;

	static constexpr bool validIndex(int idx) { return idx >= 0 && idx < invalid_index; }
	static constexpr int maxIndex() { return invalid_index - 1; }

	constexpr GenericTagId(Tag tag, int idx) : m_idx(idx), m_tag(tag) { PASSERT(validIndex(idx)); }
	constexpr GenericTagId(Tag tag, int idx, NoAssertsTag) : m_idx(idx), m_tag(tag) {}
	constexpr GenericTagId(NoInitTag) { IF_PARANOID(m_idx = uninitialized_index); }

	template <Tag tag, class UBase, int rhs_invalid_index = TagId<tag, UBase>::invalid_index,
			  EnableIf<(rhs_invalid_index <= invalid_index)>...>
	GenericTagId(TagId<tag, UBase> id) : GenericTagId(tag, id.index()) {}

	template <Tag tag, class UBase, EnableIf<(TagId<tag, UBase>::invalid_index > invalid_index)>...>
	explicit GenericTagId(TagId<tag, UBase> id) : GenericTagId(tag, id.index()) {}

	template <class UBase, unsigned ubits, class GTagId = GenericTagId<Tag, UBase, ubits>,
			  EnableIf<(GTagId::invalid_index > invalid_index)>...>
	operator GTagId() const {
		PASSERT(isInitialized());
		return GTagId(tag, m_idx);
	}

	explicit operator bool() const = delete;
	operator int() const { return PASSERT(isInitialized()), m_idx; }
	int index() const { return PASSERT(isInitialized()), m_idx; }
	Tag tag() const { return PASSERT(isInitialized()), m_tag; }

	GenericTagId(EmptyMaybe) : m_idx(invalid_index) {}
	bool validMaybe() const { return m_idx != invalid_index; }

	auto tied() const { return PASSERT(isInitialized()), tuple(m_tag, m_idx); }
	FWK_TIED_COMPARES(GenericTagId)

  private:
	bool isInitialized() const { return m_idx != uninitialized_index; }
	Base m_idx : base_bits;
	Tag m_tag;
};

// Similar to GenericTagId but with tag parameter included in it's type
template <auto tag, class Base_ /*= unsigned*/> class TagId {
  public:
	using Tag = decltype(tag);
	using Base = Base_;
	static_assert(std::is_unsigned<Base>::value);
	static_assert(sizeof(Base) <= 4);

	static constexpr int base_bits = sizeof(Base) * 8;
	static constexpr int invalid_index = min<long long>(0x7fffffff - 1, (1ull << base_bits) - 2);
	static constexpr int uninitialized_index = invalid_index + 1;

	static constexpr bool validIndex(int idx) { return idx >= 0 && idx < invalid_index; }
	static constexpr int maxIndex() { return invalid_index - 1; }

	explicit constexpr TagId(int idx) : m_idx(idx) { PASSERT(validIndex(idx)); }
	constexpr TagId(int idx, NoAssertsTag) : m_idx(idx) {}
	constexpr TagId(NoInitTag) { IF_PARANOID(m_idx = uninitialized_index); }

	template <class UBase, unsigned ubits,
			  int rhs_invalid_index = GenericTagId<Tag, UBase, ubits>::invalid_index,
			  EnableIf<rhs_invalid_index <= invalid_index>...>
	TagId(GenericTagId<Tag, UBase, ubits> id) : TagId(id.index()) {}

	template <class UBase, unsigned ubits,
			  EnableIf<(GenericTagId<Tag, UBase, ubits>::invalid_index > invalid_index)>...>
	explicit TagId(GenericTagId<Tag, UBase, ubits> id) : TagId(id.index()) {}

	explicit operator bool() const = delete;
	operator int() const { return PASSERT(isInitialized()), m_idx; }
	int index() const { return PASSERT(isInitialized()), m_idx; }

	template <class U, U u, EnableIf<!isTagConvertible(u, tag)>...>
	explicit TagId(TagId<u> rhs) : m_idx(rhs.index()) {}
	template <class U, U u, EnableIf<isTagConvertible(u, tag)>...>
	TagId(TagId<u> rhs) : m_idx(rhs.index()) {}

	TagId(EmptyMaybe) : m_idx(invalid_index) {}
	bool validMaybe() const { return m_idx != invalid_index; }

	auto tied() const { return PASSERT(isInitialized()), tie(m_idx); }
	FWK_TIED_COMPARES(TagId)

  private:
	bool isInitialized() const { return m_idx != uninitialized_index; }

	Base m_idx;
};

namespace detail {
	template <class T> constexpr bool is_tag_id = false;
	template <class Tag, Tag tag, class Base> constexpr bool is_tag_id<TagId<tag, Base>> = true;
	template <class T> constexpr None get_id_tag = none;
	template <class Tag, Tag tag, class Base> constexpr Tag get_id_tag<TagId<tag, Base>> = tag;
}

template <class DstId, class Tag, Tag src_tag, class Base, EnableIf<detail::is_tag_id<DstId>>...>
DstId castTag(const TagId<src_tag, Base> &tag) {
	return DstId(tag.index());
}

}
