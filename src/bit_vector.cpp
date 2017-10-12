// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/bit_vector.h"

namespace fwk {

BitVector::BitVector(int size) : m_data((size + base_size - 1) / base_size), m_size(size) {}

void BitVector::resize(int new_size, bool value) {
	PodVector<u32> new_data(new_size);
	memcpy(new_data.data(), m_data.data(), sizeof(base_type) * min(new_size, m_data.size()));
	if(new_data.size() > m_data.size())
		memset(new_data.data() + m_data.size(), value ? 0xff : 0,
			   (new_data.size() - m_data.size()) * sizeof(base_type));
	m_data.swap(new_data);
	m_size = new_size;
}

void BitVector::fill(bool value) {
	memset(m_data.data(), value ? 0xff : 0, m_data.size() * sizeof(base_type));
}
}
