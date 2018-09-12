// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/dtexture.h"

#include "fwk/gfx/opengl.h"
#include "fwk/gfx/texture.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

void DTexture::initialize(int msaa_samples) {
	ON_FAIL("DTexture::initialize() error; format: % size: %", m_format.id(), m_size);
	DASSERT(m_size.x >= 0 && m_size.y >= 0);
	PASSERT_GFX_THREAD();

	if(msaa_samples > 1)
		m_flags |= Opt::multisample;

	glGenTextures(1, &m_id);
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

	// TODO: finally
	// glDeleteTextures(1, &m_id);
	updateParams();
}

DTexture::DTexture(Format format, const int2 &size, int multisample_count, Flags flags)
	: m_id(0), m_size(size), m_format(format), m_flags(flags), m_has_mipmaps(false) {
	initialize(multisample_count);
}

DTexture::DTexture(Format format, const int2 &size, Flags flags)
	: m_id(0), m_size(size), m_format(format), m_flags(flags), m_has_mipmaps(false) {
	initialize(1);
}

int DTexture::glType() const {
	return m_flags & Opt::multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
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

void DTexture::generateMipmaps() {
	bind();
	DASSERT(!(m_flags & Opt::multisample));
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

	glTexSubImage2D(glType(), 0, target_pos.x, target_pos.y, size.x, size.y, format.glFormat(),
					format.glType(), pixels);
}

void DTexture::upload(CSpan<char> bytes) {
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glTexImage2D(glType(), 0, m_format.glInternal(), m_size.x, m_size.y, 0, m_format.glFormat(),
				 m_format.glType(), bytes.data());
}

void DTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_size);
	glGetTexImage(glType(), 0, m_format.glFormat(), m_format.glType(), target.data());
}

void DTexture::download(Span<char> bytes) const {
	bind();
	DASSERT(bytes.size() >= m_format.evalImageSize(m_size.x, m_size.y));
	glGetTexImage(glType(), 0, m_format.glFormat(), m_format.glType(), bytes.data());
}

void DTexture::copy(const DTexture &source, IRect src_rect, int2 target_pos) {
	DASSERT(source.contains(src_rect));
	IRect target_rect(target_pos, target_pos + src_rect.size());
	DASSERT(contains(target_rect));

	glCopyImageSubData(source.m_id, source.glType(), 0, src_rect.x(), src_rect.y(), 0, m_id,
					   glType(), 0, target_pos.x, target_pos.y, 0, src_rect.width(),
					   src_rect.height(), 0);
}

void DTexture::bind() const {
	PASSERT_GFX_THREAD();
	::glBindTexture(glType(), m_id);
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

	// TODO: handle multisampled ?
	for(int n = 0; n < set.size(); n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		if(set[n])
			::glBindTexture(set[n]->glType(), set[n]->m_id);
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
	// TODO: unbind multisample ?
	::glBindTexture(GL_TEXTURE_2D, 0);
}

IRect DTexture::rect() const { return IRect(m_size); }
bool DTexture::contains(const IRect &rect) const { return IRect(m_size).contains(rect); }
}
