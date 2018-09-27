// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_buffer.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/texture_format.h"

namespace fwk {

GL_CLASS_IMPL(GlBuffer)

static const EnumMap<BufferType, int> s_types = {
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

static const EnumMap<BufferUsage, int> s_usages = {
	GL_STREAM_DRAW, GL_STREAM_READ,  GL_STREAM_COPY,  GL_STATIC_DRAW,  GL_STATIC_READ,
	GL_STATIC_COPY, GL_DYNAMIC_DRAW, GL_DYNAMIC_READ, GL_DYNAMIC_COPY,
};

static const EnumMap<AccessMode, int> s_access_modes = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

static int toGL(ImmBufferFlags flags) {
	// 0x001, 0x002: GL_MAP_READ_BIT, GL_MAP_WRITE_BIT,
	// 0x040, 0x080: GL_MAP_PERSISTENT_BIT, GL_MAP_COHERENT_BIT
	// 0x100, 0x200: GL_DYNAMIC_STORAGE_BIT, GL_CLIENT_STORAGE_BIT
	return (flags.bits & 0x3) | ((flags.bits & ~0x3) << 4);
}

PBuffer GlBuffer::make(Type type) {
	PBuffer ref(storage.make());
	ref->m_type = type;
	return ref;
}

PBuffer GlBuffer::make(Type type, int size, BufferUsage usage) {
	auto ref = make(type);
	ref->resize(size, usage);
	return ref;
}

PBuffer GlBuffer::make(Type type, int size, ImmBufferFlags flags) {
	auto ref = make(type);
	ref->bind();
	glBufferStorage(s_types[ref->m_type], size, nullptr, toGL(flags));
	ref->m_size = size;
	return ref;
}

void GlBuffer::resize(int new_size, BufferUsage usage) {
	DASSERT(new_size >= 0);
	bind();
	glBufferData(s_types[m_type], new_size, 0, s_usages[usage]);
	m_size = new_size;
}

void GlBuffer::upload(CSpan<char> data) {
	bind();
	glBufferData(s_types[m_type], data.size(), data.data(), GL_DYNAMIC_COPY);
	m_size = data.size();
}

void GlBuffer::download(Span<char> data) const {
	DASSERT(data.size() <= m_size);
	bind();
	glGetBufferSubData(s_types[m_type], 0, data.size(), data.data());
}

void GlBuffer::clear(TextureFormat fmt, int value) {
	DASSERT((int)sizeof(value) >= fmt.bytesPerPixel());
	bind();
	glClearBufferData(s_types[m_type], fmt.glInternal(), fmt.glFormat(), fmt.glType(), &value);
}

void *GlBuffer::map(AccessMode mode) {
	bind();
	return glMapBuffer(s_types[m_type], s_access_modes[mode]);
}

bool GlBuffer::unmap() {
	bind();
	return glUnmapBuffer(s_types[m_type]);
}
bool GlBuffer::unmap(Type type) { return glUnmapBuffer(s_types[type]); }

void GlBuffer::invalidate() { glInvalidateBufferData(id()); }

void *GlBuffer::map(i64 offset, i64 size, MapFlags flags) {
	PASSERT(flags & (MapOpt::read | MapOpt::write));
	DASSERT(offset >= 0 && offset + size <= m_size);
	bind();
	return glMapBufferRange(s_types[m_type], offset, size, flags.bits);
}

void GlBuffer::flushMapped(i64 offset, i64 size) {
	bind();
	glFlushMappedBufferRange(s_types[m_type], offset, size);
}

void GlBuffer::copyTo(PBuffer target, int read_offset, int write_offset, int size) const {
	// TODO: remove these types from BufferTypes ?
	// TODO: additional checking
	glBindBuffer(GL_COPY_READ_BUFFER, id());
	glBindBuffer(GL_COPY_WRITE_BUFFER, target.id());
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, read_offset, write_offset, size);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void GlBuffer::bind() const { glBindBuffer(s_types[m_type], id()); }
void GlBuffer::unbind() const { glBindBuffer(s_types[m_type], 0); }
void GlBuffer::unbind(Type type) { glBindBuffer(s_types[type], 0); }

void GlBuffer::bindIndex(int binding_index) {
	glBindBufferBase(s_types[m_type], binding_index, id());
}

void GlBuffer::bindIndexAs(int binding_index, BufferType type) {
	glBindBufferBase(s_types[type], binding_index, id());
}

void GlBuffer::validate() {
	int bsize = 0, busage = 0;
	glGetBufferParameteriv(s_types[m_type], GL_BUFFER_SIZE, &bsize);
	glGetBufferParameteriv(s_types[m_type], GL_BUFFER_USAGE, &busage);
	ASSERT(m_size == bsize);
}
}
