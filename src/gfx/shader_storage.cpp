// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_storage.h"

#include "fwk/format.h"
#include "fwk_opengl.h"

namespace fwk {

ShaderStorage::ShaderStorage() { glGenBuffers(1, &m_handle); }

ShaderStorage::~ShaderStorage() { glDeleteBuffers(1, &m_handle); }

void ShaderStorage::resize(int new_size) {
	DASSERT(new_size >= 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, new_size, 0, GL_DYNAMIC_COPY);
	m_size = new_size;
}

void ShaderStorage::upload(CSpan<char> data) {
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, data.size(), data.data(), GL_DYNAMIC_COPY);
	m_size = data.size();
}

void ShaderStorage::download(Span<char> data) const {
	DASSERT(data.size() <= m_size);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_handle);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, data.size(), data.data());
}

void ShaderStorage::bind(int binding_index) {
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_index, m_handle);
}
}
