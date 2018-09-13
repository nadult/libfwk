// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_texture.h"

#include "fwk/gfx/opengl.h"
#include "fwk/gfx/texture.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

GL_CLASS_IMPL(GlTexture)

void GlTexture::initialize(int msaa_samples) {
	ON_FAIL("GlTexture::initialize() error; format: % size: %", m_format.id(), m_size);
	DASSERT(m_size.x >= 0 && m_size.y >= 0);
	PASSERT_GFX_THREAD();

	if(msaa_samples > 1)
		m_flags |= Opt::multisample;

	bind();

	if(m_flags & Opt::multisample) {
		if(m_flags & Opt::immutable)
			glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa_samples,
									  m_format.glInternal(), m_size.x, m_size.y, GL_TRUE);
		else
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaa_samples, m_format.glInternal(),
									m_size.x, m_size.y, GL_TRUE);
	} else {
		if(m_flags & Opt::immutable)
			glTexStorage2D(GL_TEXTURE_2D, 1, m_format.glInternal(), m_size.x, m_size.y);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, m_format.glInternal(), m_size.x, m_size.y, 0,
						 m_format.glFormat(), m_format.glType(), 0);
	}

	updateParams();
}

GlTexture::GlTexture(Format format, int2 size, Flags flags)
	: m_size(size), m_format(format), m_flags(flags) {}

PTexture GlTexture::make(Format format, const int2 &size, int multisample_count, Flags flags) {
	PTexture ref(storage.make(format, size, flags));
	ref->initialize(multisample_count);
	return ref;
}

PTexture GlTexture::make(Format format, const int2 &size, Flags flags) {
	PTexture ref(storage.make(format, size, flags));
	ref->initialize(1);
	return ref;
}

PTexture GlTexture::make(const string &name, Stream &stream) { return make(Texture(stream)); }

PTexture GlTexture::make(Format format, const Texture &tex, Flags flags) {
	PTexture ref = make(format, tex.size(), flags);
	ref->upload(tex);
	return ref;
}

PTexture GlTexture::make(Format format, const int2 &size, CSpan<float4> data, Flags flags) {
	DASSERT(data.size() >= size.x * size.y);
	PTexture ref = make(format, size, flags);
	ref->upload(TextureFormatId::rgba_f32, data.data(), size);
	return ref;
}

PTexture GlTexture::make(const Texture &tex, Flags flags) { return make(tex.format(), tex, flags); }

PTexture GlTexture::make(Format format, PTexture view_source) {
	DASSERT(view_source);
	PTexture ref(storage.make(format, view_source->size(), Flags()));
	glTextureView(ref.id(), GL_TEXTURE_2D, view_source.id(), format.glInternal(), 0, 1, 0, 1);
	return ref;
}

bool GlTexture::hasImmutableFormat() const {
	GLint ret;
	bind();
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &ret);
	return ret != 0;
}

void GlTexture::setFlags(Flags flags) {
	if(m_flags != flags) {
		DASSERT((flags & Opt::immutable) == (m_flags & Opt::immutable));
		m_flags = flags;
		updateParams();
	}
}

void GlTexture::updateParams() {
	if(m_flags & Opt::multisample)
		return;
	bind();

	int wrapping = m_flags & Opt::wrapped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(glType(), GL_TEXTURE_WRAP_S, wrapping);
	glTexParameteri(glType(), GL_TEXTURE_WRAP_T, wrapping);

	int filter = m_flags & Opt::filtered ? GL_LINEAR : GL_NEAREST;
	int min_filter = m_has_mipmaps
						 ? filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST
						 : filter;

	glTexParameteri(glType(), GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(glType(), GL_TEXTURE_MIN_FILTER, min_filter);
}

void GlTexture::generateMipmaps() {
	bind();
	DASSERT(!(m_flags & Opt::multisample));
	glGenerateMipmap(GL_TEXTURE_2D);
	m_has_mipmaps = true;
	updateParams();
}

void GlTexture::upload(const Texture &src, const int2 &target_pos) {
	upload(src.format(), src.data(), src.size(), target_pos);
}

void GlTexture::upload(Format format, const void *pixels, const int2 &size,
					   const int2 &target_pos) {
	bind();
	DASSERT(size.x + target_pos.x <= m_size.x && size.y + target_pos.y <= m_size.y);

	glTexSubImage2D(glType(), 0, target_pos.x, target_pos.y, size.x, size.y, format.glFormat(),
					format.glType(), pixels);
}

void GlTexture::upload(CSpan<char> bytes) {
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glTexImage2D(glType(), 0, m_format.glInternal(), m_size.x, m_size.y, 0, m_format.glFormat(),
				 m_format.glType(), bytes.data());
}

void GlTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_size);
	glGetTexImage(glType(), 0, m_format.glFormat(), m_format.glType(), target.data());
}

void GlTexture::download(Span<char> bytes) const {
	bind();
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glGetTexImage(glType(), 0, m_format.glFormat(), m_format.glType(), bytes.data());
}

void GlTexture::copyTo(PTexture dst, IRect src_rect, int2 dst_pos) const {
	DASSERT(dst);
	DASSERT(contains(src_rect));
	IRect dst_rect(dst_pos, dst_pos + src_rect.size());
	DASSERT(dst->contains(dst_rect));

	glCopyImageSubData(id(), glType(), 0, src_rect.x(), src_rect.y(), 0, dst.id(), dst->glType(), 0,
					   dst_pos.x, dst_pos.y, 0, src_rect.width(), src_rect.height(), 0);
}

void GlTexture::bind() const {
	PASSERT_GFX_THREAD();
	::glBindTexture(glType(), id());
}

void GlTexture::bind(CSpan<PTexture> set) {
	static int max_bind = 0;

	// TODO: handle multisampled ?
	for(int n = 0; n < set.size(); n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		if(set[n])
			::glBindTexture(set[n]->glType(), set[n].id());
		else
			::glBindTexture(GL_TEXTURE_2D, 0);
	}
	for(int n = set.size(); n < max_bind; n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		::glBindTexture(GL_TEXTURE_2D, 0);
	}
	max_bind = set.size();
	glActiveTexture(GL_TEXTURE0);
}

void GlTexture::clear(float4 value) {
	DASSERT((int)sizeof(value) >= m_format.bytesPerPixel());
	glClearTexImage(id(), 0, m_format.glFormat(), m_format.glType(), &value);
}

void GlTexture::clear(int value) {
	DASSERT((int)sizeof(value) >= m_format.bytesPerPixel());
	glClearTexImage(id(), 0, m_format.glFormat(), m_format.glType(), &value);
}

static int gl_access[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

void GlTexture::bindImage(int unit, AccessMode access, int level) {
	DASSERT(unit >= 0);
	glBindImageTexture(unit, id(), level, GL_FALSE, 0, gl_access[(int)access],
					   m_format.glInternal());
}

void GlTexture::unbind() {
	PASSERT_GFX_THREAD();
	// TODO: unbind multisample ?
	::glBindTexture(GL_TEXTURE_2D, 0);
}

IRect GlTexture::rect() const { return IRect(m_size); }
bool GlTexture::contains(const IRect &rect) const { return IRect(m_size).contains(rect); }

int GlTexture::glType() const {
	return m_flags & Opt::multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
}
}
