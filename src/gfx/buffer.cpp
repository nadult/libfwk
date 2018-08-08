// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/buffer.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/texture_format.h"

namespace fwk {

EnumMap<BufferType, int> s_types = {
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER,
	GL_COPY_READ_BUFFER,
	GL_COPY_WRITE_BUFFER,
	GL_PIXEL_UNPACK_BUFFER,
	GL_PIXEL_PACK_BUFFER,
	GL_QUERY_BUFFER,
	GL_TEXTURE_BUFFER,
	GL_TRANSFORM_FEEDBACK_BUFFER,
	GL_UNIFORM_BUFFER,
	GL_DRAW_INDIRECT_BUFFER,
	GL_ATOMIC_COUNTER_BUFFER,
	GL_DISPATCH_INDIRECT_BUFFER,
	GL_SHADER_STORAGE_BUFFER,
};

EnumMap<AccessMode, int> s_access_modes = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

Buffer::Buffer() { glGenBuffers(1, &m_handle); }

Buffer::~Buffer() { glDeleteBuffers(1, &m_handle); }

void Buffer::resize(int new_size) {
	DASSERT(new_size >= 0);
	glBindBuffer(s_types[m_type], m_handle);
	glBufferData(s_types[m_type], new_size, 0, GL_DYNAMIC_COPY);
	m_size = new_size;
}

void Buffer::upload(CSpan<char> data) {
	glBindBuffer(s_types[m_type], m_handle);
	glBufferData(s_types[m_type], data.size(), data.data(), GL_DYNAMIC_COPY);
	m_size = data.size();
}

void Buffer::download(Span<char> data) const {
	DASSERT(data.size() <= m_size);
	glBindBuffer(s_types[m_type], m_handle);
	glGetBufferSubData(s_types[m_type], 0, data.size(), data.data());
}

void Buffer::clear(TextureFormat fmt, int value) {
	DASSERT((int)sizeof(value) >= fmt.bytesPerPixel());
	glBindBuffer(s_types[m_type], m_handle);
	glClearBufferData(s_types[m_type], fmt.glInternal(), fmt.glFormat(), fmt.glType(), &value);
}

void *Buffer::map(AccessMode mode) { return glMapBuffer(s_types[m_type], s_access_modes[mode]); }

bool Buffer::unmap() { return glUnmapBuffer(s_types[m_type]); }
bool Buffer::unmap(Type type) { return glUnmapBuffer(s_types[type]); }

void *Buffer::map(i64 offset, i64 size, MapFlags flags) {
	PASSERT(flags & (MapBit::read | MapBit::write));
	return glMapBufferRange(s_types[m_type], offset, size, flags.bits);
}

void Buffer::flushMapped(i64 offset, i64 size) {
	glFlushMappedBufferRange(s_types[m_type], offset, size);
}

void Buffer::bind() const { glBindBuffer(s_types[m_type], m_handle); }
void Buffer::unbind() const { glBindBuffer(s_types[m_type], 0); }
void Buffer::unbind(Type type) { glBindBuffer(s_types[type], 0); }

void Buffer::bindIndex(int binding_index) {
	glBindBufferBase(s_types[m_type], binding_index, m_handle);
}
}
