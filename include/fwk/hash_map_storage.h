// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/memory.h"
#include "fwk/sys_base.h"

namespace fwk {

// Checks wheter type can hold intrusive hash info
template <class T>
static constexpr bool intrusive_hash_type =
	Intrusive::CanHold<T, Intrusive::ETag::deleted_hash>::value
		&&Intrusive::CanHold<T, Intrusive::ETag::unused_hash>::value;

template <class Key, class Value> struct KeyValue {
	explicit operator Pair<Key, Value>() const { return {key, value}; }

	Key key;
	Value value;
};

// HashMap storage where keys and values are stored together.
// For small values & keys which can hold intrusive info.
template <class Key, class Value> struct HashMapStoragePaired {
	using KeyValue = fwk::KeyValue<const Key, Value>;
	static_assert(intrusive_hash_type<Key>);
	static_assert(std::is_trivially_destructible<Key>::value,
				  "Keys when constructed with special values should be trivially destructible");

	static constexpr bool keeps_hashes = false, keeps_pairs = true;
	static constexpr auto memory_unit = sizeof(KeyValue);

	auto &keyValue(int idx) const ALWAYS_INLINE { return key_values[idx]; }
	auto &keyValue(int idx) ALWAYS_INLINE { return key_values[idx]; }
	const Key &key(int idx) const ALWAYS_INLINE { return key_values[idx].key; }
	Value &value(int idx) ALWAYS_INLINE { return key_values[idx].value; }
	const Value &value(int idx) const ALWAYS_INLINE { return key_values[idx].value; }

	bool compareKey(int idx, const Key &key, u32 hash) const { return key_values[idx].key == key; }
	bool isDeleted(int idx) const { return key_values[idx].key.holds(Intrusive::DeletedHash()); }
	bool isUnused(int idx) const { return key_values[idx].key.holds(Intrusive::UnusedHash()); }
	bool isValid(int idx) const { return !isDeleted(idx) && !isUnused(idx); }

	template <class... Args> void construct(int idx, u32, const Key &key, Args &&... args) {
		new((Key *)&key_values[idx]) KeyValue{key, Value{std::forward<Args>(args)...}};
	}

	void destruct(int idx) { key_values[idx].~KeyValue(); }
	void markDeleted(int idx) ALWAYS_INLINE {
		new((Key *)&key_values[idx].key) Key(Intrusive::DeletedHash());
	}
	void markUnused(int idx) ALWAYS_INLINE {
		new((Key *)&key_values[idx].key) Key(Intrusive::UnusedHash());
	}

	static HashMapStoragePaired allocate(int new_capacity) {
		auto *new_key_values =
			static_cast<KeyValue *>(fwk::allocate(new_capacity * sizeof(KeyValue)));
		for(int n = 0; n < new_capacity; n++)
			new((Key *)&new_key_values[n].key) Key(Intrusive::UnusedHash());
		return {new_key_values};
	}

	void deallocate() {
		if((KeyValue_ *)key_values != &s_empty_node)
			fwk::deallocate(key_values);
	}

	struct KeyValue_ {
		KeyValue_() {}
		~KeyValue_() {}
		Key key{Intrusive::UnusedHash()};
		union {
			Value value;
		};
	};
	static_assert(sizeof(KeyValue_) == sizeof(KeyValue));

	static inline KeyValue_ s_empty_node;
	KeyValue *key_values = reinterpret_cast<KeyValue *>(&s_empty_node);
};

// HashMap storage where keys and values are stored separately.
// For keys which can hold intrusive info.
template <class Key, class Value> struct HashMapStorageSeparated {
	using KeyValue = fwk::KeyValue<const Key, Value>;
	static_assert(intrusive_hash_type<Key>);
	static_assert(std::is_trivially_destructible<Key>::value,
				  "Keys when constructed with special values should be trivially destructible");

	static constexpr bool keeps_hashes = false, keeps_pairs = false;
	static constexpr auto memory_unit = sizeof(Key) + sizeof(Value);

	KeyValue keyValue(int idx) const { return {keys[idx], values[idx]}; }
	const Key &key(int idx) const ALWAYS_INLINE { return keys[idx]; }
	Value &value(int idx) ALWAYS_INLINE { return values[idx]; }
	const Value &value(int idx) const ALWAYS_INLINE { return values[idx]; }

	bool compareKey(int idx, const Key &key, u32 hash) const { return keys[idx] == key; }
	bool isDeleted(int idx) const { return keys[idx].holds(Intrusive::DeletedHash()); }
	bool isUnused(int idx) const { return keys[idx].holds(Intrusive::UnusedHash()); }
	bool isValid(int idx) const { return !isDeleted(idx) && !isUnused(idx); }

	template <class... Args> void construct(int idx, u32, const Key &key, Args &&... args) {
		new(&keys[idx]) Key(key);
		new(&values[idx]) Value{std::forward<Args>(args)...};
	}

	void destruct(int idx) { keys[idx].~Key(), values[idx].~Value(); }
	void markDeleted(int idx) ALWAYS_INLINE { new(&keys[idx]) Key(Intrusive::DeletedHash()); }
	void markUnused(int idx) ALWAYS_INLINE { new(&keys[idx]) Key(Intrusive::UnusedHash()); }

	static HashMapStorageSeparated allocate(int new_capacity) {
		auto *new_keys = static_cast<Key *>(fwk::allocate(new_capacity * sizeof(Key)));
		for(int n = 0; n < new_capacity; n++)
			new(&new_keys[n]) Key(Intrusive::UnusedHash());
		auto *new_values = (Value *)fwk::allocate(new_capacity * sizeof(Value));
		return {new_keys, new_values};
	}

	void deallocate() {
		if(keys != &s_empty_node)
			fwk::deallocate(keys);
		fwk::deallocate(values);
	}
	static inline Key s_empty_node{Intrusive::UnusedHash()};
	Key *keys = &s_empty_node;
	Value *values = nullptr;
};

// HashMap storage where keys and values are together and hashes are stored additionally.
// Should be used when keys are big (>4 bytes) and their comparison is costly.
template <class Key, class Value> struct HashMapStoragePairedWithHashes {
	using KeyValue = fwk::KeyValue<const Key, Value>;
	static constexpr bool keeps_hashes = true, keeps_pairs = true;
	static constexpr auto memory_unit = sizeof(u32) + sizeof(KeyValue);

	auto &keyValue(int idx) const ALWAYS_INLINE { return key_values[idx]; }
	auto &keyValue(int idx) ALWAYS_INLINE { return key_values[idx]; }
	const Key &key(int idx) const ALWAYS_INLINE { return key_values[idx].key; }
	Value &value(int idx) ALWAYS_INLINE { return key_values[idx].value; }
	const Value &value(int idx) const ALWAYS_INLINE { return key_values[idx].value; }

	bool compareKey(int idx, const Key &key, u32 hash) const {
		return hashes[idx] == hash && key_values[idx].key == key;
	}
	bool isDeleted(int idx) const ALWAYS_INLINE { return hashes[idx] == deleted_hash; }
	bool isUnused(int idx) const ALWAYS_INLINE { return hashes[idx] == unused_hash; }
	bool isValid(int idx) const ALWAYS_INLINE { return hashes[idx] < deleted_hash; }

	static HashMapStoragePairedWithHashes allocate(int new_capacity) {
		u32 *new_hashes = static_cast<u32 *>(fwk::allocate(new_capacity * sizeof(u32)));
		for(int n = 0; n < new_capacity; n++)
			new_hashes[n] = unused_hash;
		auto *new_key_values = (KeyValue *)fwk::allocate(new_capacity * sizeof(KeyValue));
		return {new_hashes, new_key_values};
	}

	void deallocate() {
		if(hashes != &s_empty_node)
			fwk::deallocate(hashes);
		fwk::deallocate(key_values);
	}

	template <class... Args> void construct(int idx, u32 hash, const Key &key, Args &&... args) {
		new((Key *)&key_values[idx]) KeyValue{key, Value{std::forward<Args>(args)...}};
		hashes[idx] = hash;
	}

	void destruct(int idx) { key_values[idx].~KeyValue(); }
	void markDeleted(int idx) ALWAYS_INLINE { hashes[idx] = deleted_hash; }
	void markUnused(int idx) ALWAYS_INLINE { hashes[idx] = unused_hash; }

	static constexpr u32 unused_hash = 0xffffffff;
	static constexpr u32 deleted_hash = 0xfffffffe;

	static inline u32 s_empty_node = unused_hash;
	u32 *hashes = &s_empty_node;
	KeyValue *key_values = nullptr;
};
}
