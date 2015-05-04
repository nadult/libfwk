/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

static int s_current_tex = 0;

//TODO: what if rendering device is recreated?
//Maybe those textures should be kept inside the device?
//Similar case for VertexBuffers and VertexArrays

DTexture::DTexture(const string &name, Stream &stream) :DTexture() {
	load(stream);
}

DTexture::DTexture()
	: m_id(0), m_width(0), m_height(0), m_format(TI_Unknown), m_is_wrapped(false),
	  m_is_filtered(true), m_has_mipmaps(false), m_is_dirty(false) {}

DTexture::~DTexture() { clear(); }

void DTexture::load(Stream &sr) {
	Texture temp;
	sr >> temp;
	set(temp);
}

void DTexture::setWrapping(bool enable) {
	m_is_wrapped = enable;
	m_is_dirty = true;
}

void DTexture::setFiltering(bool enable) {
	m_is_filtered = enable;
	m_is_dirty = true;
}

void DTexture::generateMipmaps() {
	if(!isValid())
		return;

	bind();
	glGenerateMipmap(GL_TEXTURE_2D);
	m_has_mipmaps = true;
	m_is_dirty = true;
	bind();
}

void DTexture::resize(TextureFormat format, int width, int height) {
	if(!m_id) {
		GLuint gl_id;
		glGenTextures(1, &gl_id);
		testGlError("glGenTextures");
		m_id = (int)gl_id;
		m_is_dirty = true;
	}

	if(m_width == width && m_height == height && m_format == format)
		return;

	try {
		bind();
		glTexImage2D(GL_TEXTURE_2D, 0, format.glInternal(), width, height, 0, format.glFormat(),
					 format.glType(), 0);
		m_has_mipmaps = false;
		testGlError("glTexImage2D");

	} catch(const Exception &ex) {
		clear();
		THROW("Error while creating texture (width: %d height: %d format id: %d): %s", width,
			  height, (int)format.ident(), ex.what());
	}

	m_width = width;
	m_height = height;
	m_format = format;
}

void DTexture::clear() {
	if(m_id) {
		GLuint gl_id = m_id;
		glDeleteTextures(1, &gl_id);
		if(s_current_tex == m_id)
			s_current_tex = 0;
		m_id = 0;
	}

	m_width = m_height = 0;
	m_format = TI_Unknown;
}

void DTexture::set(const Texture &src) {
	resize(src.format(), src.width(), src.height());
	upload(src, int2(0, 0));
}

void DTexture::upload(const Texture &src, const int2 &target_pos) {
	DASSERT(isValid());
	DASSERT(src.format() == m_format);
	upload(src.data(), src.size(), target_pos);
}

void DTexture::upload(const void *pixels, const int2 &size, const int2 &target_pos) {
	bind();
	DASSERT(size.x + target_pos.x <= m_width && size.y + target_pos.y <= m_height);

	glTexSubImage2D(GL_TEXTURE_2D, 0, target_pos.x, target_pos.y, size.x, size.y,
					m_format.glFormat(), m_format.glType(), pixels);
}

void DTexture::download(Texture &target) const {
	bind();
	DASSERT(m_format == target.format());
	target.resize(m_width, m_height);
	glGetTexImage(GL_TEXTURE_2D, 0, m_format.glFormat(), m_format.glType(), target.data());
}

void DTexture::blit(DTexture &target, const IRect &src_rect, const int2 &target_pos) const {
	// TODO: use PBO's
	THROW("blitting from one DTexture to another not supported yet");
}

void DTexture::bind() const {
	DASSERT(isValid());
	if(m_id != s_current_tex) {
		::glBindTexture(GL_TEXTURE_2D, m_id);
		s_current_tex = m_id;
	}
	if(m_is_dirty) {
		int wrapping = m_is_wrapped ? GL_CLAMP_TO_EDGE : GL_REPEAT;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);

		int filter = m_is_filtered ? GL_LINEAR : GL_NEAREST;
		int min_filter =
			m_has_mipmaps
				? filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST
				: filter;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
		m_is_dirty = false;
	}
}

void DTexture::unbind() {
	if(s_current_tex) {
		::glBindTexture(GL_TEXTURE_2D, 0);
		s_current_tex = 0;
	}
}
}
