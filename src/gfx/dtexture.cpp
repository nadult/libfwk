// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/dtexture.h"

#include "fwk/gfx/opengl.h"
#include "fwk/gfx/texture.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

DTexture::DTexture(Format format, const int2 &size, Flags flags)
	: m_id(0), m_size(size), m_format(format), m_flags(flags), m_has_mipmaps(false) {
	DASSERT(size.x >= 0 && size.y >= 0);
	PASSERT_GFX_THREAD();

	ON_FAIL("DTexture::DTexture() error; format: % size: %", format.id(), size);

	{
		glGenTextures(1, &m_id);
		testGlError("glGenTextures");

		bind();
		if(flags & Opt::immutable) {
			glTexStorage2D(GL_TEXTURE_2D, 1, m_format.glInternal(), m_size.x, m_size.y);
			testGlError("glTexStorage2D");
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, format.glInternal(), m_size.x, m_size.y, 0,
						 format.glFormat(), format.glType(), 0);
		}

		m_has_mipmaps = false;
		testGlError("glTexImage2D");
	}
	// TODO: finally
	// glDeleteTextures(1, &m_id);

	updateParams();
}

DTexture::DTexture(const string &name, Stream &stream) : DTexture(Texture(stream)) {}
DTexture::DTexture(Format format, const Texture &tex, Flags flags)
	: DTexture(format, tex.size(), flags) {
	upload(tex);
}
DTexture::DTexture(Format format, const int2 &size, CSpan<float4> data, Flags flags)
	: DTexture(format, size, flags) {
	DASSERT(data.size() >= size.x * size.y);
	upload(TextureFormatId::rgba_f32, data.data(), size);
}
DTexture::DTexture(const Texture &tex, Flags flags) : DTexture(tex.format(), tex, flags) {}

DTexture::DTexture(Format fmt, const DTexture &view_source)
	: m_size(view_source.size()), m_format(fmt), m_has_mipmaps(false) {
	glGenTextures(1, &m_id);
	testGlError("glGenTextures");

	glTextureView(m_id, GL_TEXTURE_2D, view_source.m_id, fmt.glInternal(), 0, 1, 0, 1);
	testGlError("glTextureView");
}

bool DTexture::hasImmutableFormat() const {
	GLint ret;
	bind();
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &ret);
	return ret != 0;
}

/*
DTexture::DTexture(DTexture &&rhs)
	: m_id(rhs.m_id), m_size(rhs.m_size), m_format(rhs.m_format), m_config(rhs.m_config),
	  m_has_mipmaps(rhs.m_has_mipmaps) {
	rhs.m_id = 0;
	rhs.m_size = int2();
}*/

DTexture::~DTexture() {
	PASSERT_GFX_THREAD();
	if(m_id)
		glDeleteTextures(1, &m_id);
}

void DTexture::setFlags(Flags flags) {
	if(m_flags != flags) {
		DASSERT((flags & Opt::immutable) == (m_flags & Opt::immutable));
		m_flags = flags;
		updateParams();
	}
}

void DTexture::updateParams() {
	bind();

	int wrapping = m_flags & Opt::wrapped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);

	int filter = m_flags & Opt::filtered ? GL_LINEAR : GL_NEAREST;
	int min_filter = m_has_mipmaps
						 ? filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST
						 : filter;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

void DTexture::generateMipmaps() {
	bind();
	glGenerateMipmap(GL_TEXTURE_2D);
	m_has_mipmaps = true;
	updateParams();
}

void DTexture::upload(const Texture &src, const int2 &target_pos) {
	upload(src.format(), src.data(), src.size(), target_pos);
}

void DTexture::upload(Format format, const void *pixels, const int2 &size, const int2 &target_pos) {
	bind();
	DASSERT(size.x + target_pos.x <= m_size.x && size.y + target_pos.y <= m_size.y);

	glTexSubImage2D(GL_TEXTURE_2D, 0, target_pos.x, target_pos.y, size.x, size.y, format.glFormat(),
					format.glType(), pixels);
}

void DTexture::upload(CSpan<char> bytes) {
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glTexImage2D(GL_TEXTURE_2D, 0, m_format.glInternal(), m_size.x, m_size.y, 0,
				 m_format.glFormat(), m_format.glType(), bytes.data());
}

void DTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_size);
	glGetTexImage(GL_TEXTURE_2D, 0, m_format.glFormat(), m_format.glType(), target.data());
}

void DTexture::download(Span<char> bytes) const {
	bind();
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glGetTexImage(GL_TEXTURE_2D, 0, m_format.glFormat(), m_format.glType(), bytes.data());
}

void DTexture::copy(const DTexture &source, IRect src_rect, int2 target_pos) {
	DASSERT(source.contains(src_rect));
	IRect target_rect(target_pos, target_pos + src_rect.size());
	DASSERT(contains(target_rect));

	glCopyImageSubData(source.m_id, GL_TEXTURE_2D, 0, src_rect.x(), src_rect.y(), 0, m_id,
					   GL_TEXTURE_2D, 0, target_pos.x, target_pos.y, 0, src_rect.width(),
					   src_rect.height(), 0);
}

void DTexture::bind() const {
	PASSERT_GFX_THREAD();
	::glBindTexture(GL_TEXTURE_2D, m_id);
}

void DTexture::bind(const vector<immutable_ptr<DTexture>> &set) {
	vector<const DTexture *> temp;
	temp.reserve(set.size());

	for(auto &tex : set)
		temp.emplace_back(tex.get());
	bind(temp);
}

void DTexture::bind(const vector<const DTexture *> &set) {
	static int max_bind = 0;

	for(int n = 0; n < set.size(); n++) {
		DASSERT(set[n]);
		glActiveTexture(GL_TEXTURE0 + n);
		::glBindTexture(GL_TEXTURE_2D, set[n]->m_id);
	}
	for(int n = set.size(); n < max_bind; n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		::glBindTexture(GL_TEXTURE_2D, 0);
	}
	max_bind = set.size();
	glActiveTexture(GL_TEXTURE0);
}

void DTexture::clear(float4 value) {
	DASSERT((int)sizeof(value) >= m_format.bytesPerPixel());
	glClearTexImage(m_id, 0, m_format.glFormat(), m_format.glType(), &value);
}

void DTexture::clear(int value) {
	DASSERT((int)sizeof(value) >= m_format.bytesPerPixel());
	glClearTexImage(m_id, 0, m_format.glFormat(), m_format.glType(), &value);
}

static int gl_access[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

void DTexture::bindImage(int unit, AccessMode access, int level) {
	DASSERT(unit >= 0);
	glBindImageTexture(unit, m_id, level, GL_FALSE, 0, gl_access[(int)access],
					   m_format.glInternal());
}

void DTexture::unbind() {
	PASSERT_GFX_THREAD();
	::glBindTexture(GL_TEXTURE_2D, 0);
}

IRect DTexture::rect() const { return IRect(m_size); }
bool DTexture::contains(const IRect &rect) const { return IRect(m_size).contains(rect); }
}
