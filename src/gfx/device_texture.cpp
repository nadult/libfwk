// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/dtexture.h"
#include "fwk/gfx/texture.h"
#include "fwk/sys/on_fail.h"
#include "fwk_opengl.h"

namespace fwk {

DTexture::DTexture(Format format, const int2 &size, const Config &config)
	: m_id(0), m_size(size), m_format(format), m_config(config), m_has_mipmaps(false) {
	DASSERT(size.x >= 0 && size.y >= 0);
	PASSERT_GFX_THREAD();

	ON_FAIL("DTexture::DTexture() error; format: % size: %", size, format.id());

	{
		glGenTextures(1, &m_id);
		testGlError("glGenTextures");

		bind();
		glTexImage2D(GL_TEXTURE_2D, 0, format.glInternal(), m_size.x, m_size.y, 0,
					 format.glFormat(), format.glType(), 0);
		m_has_mipmaps = false;
		testGlError("glTexImage2D");
	}
	// TODO: finally
	// glDeleteTextures(1, &m_id);

	updateConfig();
}

DTexture::DTexture(const string &name, Stream &stream) : DTexture(Texture(stream)) {}
DTexture::DTexture(Format format, const Texture &tex, const Config &config)
	: DTexture(format, tex.size(), config) {
	upload(tex);
}
DTexture::DTexture(Format format, const int2 &size, CSpan<float4> data, const Config &config)
	: DTexture(format, size, config) {
	DASSERT(data.size() >= size.x * size.y);
	upload(TextureFormatId::rgba_f32, data.data(), size);
}
DTexture::DTexture(const Texture &tex, const Config &config)
	: DTexture(tex.format(), tex, config) {}

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

void DTexture::setConfig(const Config &config) {
	if(m_config != config) {
		m_config = config;
		updateConfig();
	}
}

void DTexture::generateMipmaps() {
	bind();
	glGenerateMipmap(GL_TEXTURE_2D);
	m_has_mipmaps = true;
	updateConfig();
}

void DTexture::updateConfig() {
	bind();

	int wrapping = m_config.flags & Config::flag_wrapped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);

	int filter = m_config.flags & Config::flag_filtered ? GL_LINEAR : GL_NEAREST;
	int min_filter = m_has_mipmaps
						 ? filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST
						 : filter;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
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

void DTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_size);
	glGetTexImage(GL_TEXTURE_2D, 0, m_format.glFormat(), m_format.glType(), target.data());
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

void DTexture::unbind() {
	PASSERT_GFX_THREAD();
	::glBindTexture(GL_TEXTURE_2D, 0);
}
}
