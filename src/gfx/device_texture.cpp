/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

// TODO: what if rendering device is recreated?
// Maybe those textures should be kept inside the device?
// Similar case for VertexBuffers and VertexArrays

DTexture::DTexture(TextureFormat format, const int2 &size, const TextureConfig &config)
	: m_id(0), m_size(size), m_format(format), m_config(config), m_has_mipmaps(false) {

	try {
		glGenTextures(1, &m_id);
		testGlError("glGenTextures");

		bind();
		glTexImage2D(GL_TEXTURE_2D, 0, format.glInternal(), m_size.x, m_size.y, 0,
					 format.glFormat(), format.glType(), 0);
		m_has_mipmaps = false;
		testGlError("glTexImage2D");

	} catch(const Exception &ex) {
		glDeleteTextures(1, &m_id);
		THROW("Error while creating texture (width: %d height: %d format: %s): %s", m_size.x,
			  m_size.y, toString(format.id()), ex.what());
	}

	updateConfig();
}

DTexture::DTexture(const string &name, Stream &stream) : DTexture(Texture(stream)) {}
DTexture::DTexture(const Texture &tex, const TextureConfig &config)
	: DTexture(tex.format(), tex.size(), config) {
	upload(tex, int2(0, 0));
}

DTexture::DTexture(DTexture &&rhs)
	: m_id(rhs.m_id), m_size(rhs.m_size), m_format(rhs.m_format), m_config(rhs.m_config),
	  m_has_mipmaps(rhs.m_has_mipmaps) {
	rhs.m_id = 0;
	rhs.m_size = int2();
}

DTexture::~DTexture() {
	if(m_id)
		glDeleteTextures(1, &m_id);
}

void DTexture::setConfig(const TextureConfig &config) {
	if(!isValid())
		return;

	if(m_config != config) {
		m_config = config;
		updateConfig();
	}
}

void DTexture::generateMipmaps() {
	if(!isValid())
		return;

	bind();
	glGenerateMipmap(GL_TEXTURE_2D);
	m_has_mipmaps = true;
	updateConfig();
}

void DTexture::updateConfig() {
	bind();

	int wrapping = m_config.flags & TextureConfig::flag_wrapped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);

	int filter = m_config.flags & TextureConfig::flag_filtered ? GL_LINEAR : GL_NEAREST;
	int min_filter = m_has_mipmaps
						 ? filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST
						 : filter;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

void DTexture::upload(const Texture &src, const int2 &target_pos) {
	DASSERT(isValid());
	DASSERT(src.format() == m_format);
	upload(src.data(), src.size(), target_pos);
}

void DTexture::upload(const void *pixels, const int2 &size, const int2 &target_pos) {
	bind();
	DASSERT(size.x + target_pos.x <= m_size.x && size.y + target_pos.y <= m_size.y);

	glTexSubImage2D(GL_TEXTURE_2D, 0, target_pos.x, target_pos.y, size.x, size.y,
					m_format.glFormat(), m_format.glType(), pixels);
}

void DTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_size.x, m_size.y);
	glGetTexImage(GL_TEXTURE_2D, 0, m_format.glFormat(), m_format.glType(), target.data());
}

void DTexture::bind() const {
	DASSERT(isValid());
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

	for(int n = 0; n < (int)set.size(); n++) {
		DASSERT(set[n]);
		glActiveTexture(GL_TEXTURE0 + n);
		::glBindTexture(GL_TEXTURE_2D, set[n]->m_id);
	}
	for(int n = (int)set.size(); n < max_bind; n++) {
		glActiveTexture(GL_TEXTURE0 + n);
		::glBindTexture(GL_TEXTURE_2D, 0);
	}
	max_bind = (int)set.size();
	glActiveTexture(GL_TEXTURE0);
}

void DTexture::unbind() { ::glBindTexture(GL_TEXTURE_2D, 0); }
}
