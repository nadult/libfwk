// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/pod_vector.h"

namespace fwk {

class BitVector {
  public:
	typedef u32 base_type;
	enum {
		base_shift = 5,
		base_size = 32,
	};

	struct Bit {
		Bit(base_type &base, int bit_index) : base(base), bit_index(bit_index) {}
		operator bool() const { return base & (base_type(1) << bit_index); }

		void operator=(bool value) {
			base = (base & ~(base_type(1) << bit_index)) | ((base_type)value << bit_index);
		}

	  protected:
		base_type &base;
		int bit_index;
	};

	explicit BitVector(int size = 0);
	void resize(int new_size, bool value = false);

	int size() const { return m_size; }
	int baseSize() const { return m_data.size(); }

	void fill(bool value);

	const PodVector<base_type> &data() const { return m_data; }
	PodVector<base_type> &data() { return m_data; }

	bool operator[](int idx) const {
		PASSERT(inRange(idx, 0, m_size));
		return m_data[idx >> base_shift] & (1 << (idx & (base_size - 1)));
	}

	Bit operator[](int idx) {
		PASSERT(inRange(idx, 0, m_size));
		return Bit(m_data[idx >> base_shift], idx & (base_size - 1));
	}

	bool any(int base_idx) const { return m_data[base_idx] != base_type(0); }
	bool all(int base_idx) const { return m_data[base_idx] == ~base_type(0); }

  protected:
	PodVector<base_type> m_data;
	int m_size;
};
}
