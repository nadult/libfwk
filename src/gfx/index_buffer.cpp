/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

IndexBuffer::IndexBuffer(const vector<uint> &indices) : m_handle(0), m_size((int)indices.size()) {
	if(!m_size)
		return;
	uint max_index_value = 0;
	for(auto idx : indices)
		max_index_value = max(max_index_value, idx);
	ASSERT(max_index_value < IndexBuffer::max_index_value);

	bool is_small = max_index_value <= 255;
	m_index_type = is_small ? type_ubyte : type_ushort;
	m_index_size = is_small ? 1 : 2;

	glGenBuffers(1, &m_handle);
	testGlError("glGenBuffers");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);
	if(is_small) {
		PodArray<u8> data(indices.size());
		for(int n = 0; n < data.size(); n++)
			data[n] = indices[n];
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_size * m_index_size, data.data(), GL_STATIC_DRAW);
	} else {
		PodArray<u16> data(indices.size());
		for(int n = 0; n < data.size(); n++)
			data[n] = indices[n];
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_size * m_index_size, data.data(), GL_STATIC_DRAW);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

vector<uint> IndexBuffer::getData() const {
	vector<uint> out(m_size);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);
	if(m_index_type == type_ubyte) {
		PodArray<u8> data(m_size);
		glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m_size * m_index_size, data.data());
		for(int n = 0; n < data.size(); n++)
			out[n] = data[n];
	} else { // ushort
		PodArray<u16> data(m_size);
		glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m_size * m_index_size, data.data());
		for(int n = 0; n < data.size(); n++)
			out[n] = data[n];
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return out;
}

IndexBuffer::~IndexBuffer() {
	if(m_handle)
		glDeleteBuffers(1, &m_handle);
}
}
