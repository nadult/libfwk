// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_base.h"

#include <cstring>
#include <stdarg.h>

namespace fwk {

TextFormatter::TextFormatter(int size) : m_offset(0), m_data(size) {
	DASSERT(size > 0);
	m_data[0] = 0;
}

void TextFormatter::operator()(const char *format, ...) {
	bool realloc_needed = false;

	do {
		va_list ap;
		va_start(ap, format);
		int offset = vsnprintf(&m_data[m_offset], m_data.size() - m_offset, format, ap);
		va_end(ap);

		if(offset < 0) {
			m_data[m_offset] = 0;
			THROW("Textformatter: error while encoding");
		}

		if(m_offset + offset >= (int)m_data.size()) {
			m_data[m_offset] = 0;
			PodArray<char> new_data((m_offset + offset + 1) * (m_offset == 0 ? 1 : 2));
			memcpy(new_data.data(), m_data.data(), m_offset + 1);
			new_data.swap(m_data);
			realloc_needed = true;
		} else {
			m_offset += offset;
			realloc_needed = false;
		}
	} while(realloc_needed);
}
}
