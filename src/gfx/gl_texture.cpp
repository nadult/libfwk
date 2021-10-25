// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_texture.h"

#include "fwk/gfx/compressed_image.h"
#include "fwk/gfx/float_image.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/image.h"
#include "fwk/gfx/opengl.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/on_fail.h"
#include <GL/gl.h>
#include <GL/glext.h>

// TODO: cleanup here
// TODO: some limits on data allocated in OpenGL?

// TODO: better way to fix it?
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif

namespace fwk {

GL_CLASS_IMPL(GlTexture)

using FilterOpt = TextureFilterOpt;
using WrapOpt = TextureWrapOpt;

static const EnumMap<WrapOpt, int> wrap_ids = {{
	{WrapOpt::clamp_to_edge, GL_CLAMP_TO_EDGE},
	{WrapOpt::clamp_to_border, GL_CLAMP_TO_BORDER},
	{WrapOpt::repeat, GL_REPEAT},
	{WrapOpt::mirror_repeat, GL_MIRRORED_REPEAT},
	{WrapOpt::mirror_clamp_to_edge, GL_MIRROR_CLAMP_TO_EDGE},
}};

static const EnumMap<TextureType, int> type_ids = {
	{{TextureType::tex_1d, GL_TEXTURE_1D},
	 {TextureType::tex_2d, GL_TEXTURE_2D},
	 {TextureType::tex_3d, GL_TEXTURE_3D},
	 {TextureType::buffer_1d, GL_TEXTURE_BUFFER},
	 {TextureType::array_1d, GL_TEXTURE_1D_ARRAY},
	 {TextureType::array_2d, GL_TEXTURE_2D_ARRAY},
	 {TextureType::tex_2d_ms, GL_TEXTURE_2D_MULTISAMPLE},
	 {TextureType::array_2d_ms, GL_TEXTURE_2D_MULTISAMPLE_ARRAY}}};

GlTexture::GlTexture(int3 size, int num_levels, Format format, Type type)
	: m_width(size.x), m_height(size.y), m_depth(size.z), m_num_levels(num_levels),
	  m_format(format), m_type(type) {
	if(isMultisampled()) {
		DASSERT(num_levels == 1);
	} else {
		int max_dim = max(size.x, size.y);
		if(type == Type::tex_3d)
			max_dim = max(max_dim, size.z);
		DASSERT(num_levels >= 1);
		DASSERT(num_levels <= maxMipmapLevels(max_dim));
	}
	int max_tex_size = gl_info->limits[GlLimit::max_texture_size];
	DASSERT(size.x >= 1 && size.x <= max_tex_size);
	DASSERT(size.y >= 1 && size.y <= max_tex_size);
	if(isOneOf(type, Type::tex_1d, Type::buffer_1d)) // 1D
		DASSERT(size.y == 1 && size.z == 1);
	else if(isOneOf(type, Type::tex_2d, Type::tex_2d_ms, Type::array_1d)) // 2D
		DASSERT(size.z == 1);
	else {
		int max_3d_tex_size = gl_info->limits[GlLimit::max_texture_3d_size];
		DASSERT(size.z >= 1 && size.z <= max_3d_tex_size);
	}
}

Ex<PTexture> GlTexture::load(ZStr path, bool generate_mipmaps) {
	auto image = EX_PASS(Image::load(path));
	int num_mips = generate_mipmaps ? maxMipmapLevels(image.size()) : 1;
	auto tex = make(image.format(), image.size(), num_mips);
	tex->upload(image, 0);
	if(generate_mipmaps)
		tex->generateMipmaps();
	return tex;
}

PTexture GlTexture::make(Type type, Format format, const int3 &size, int num_levels) {
	DASSERT(!isOneOf(type, Type::buffer_1d, Type::tex_2d_ms, Type::array_2d_ms));
	PTexture ref(storage.make(size, num_levels, format, type));
	ref->bind();
	auto gl_type = ref->glType();
	if(type == Type::tex_1d)
		glTexStorage1D(gl_type, num_levels, glInternalFormat(format), size.x);
	else if(isOneOf(type, Type::tex_2d, Type::array_1d))
		glTexStorage2D(gl_type, num_levels, glInternalFormat(format), size.x, size.y);
	else if(isOneOf(type, Type::tex_3d, Type::array_2d))
		glTexStorage3D(gl_type, num_levels, glInternalFormat(format), size.x, size.y, size.z);
	testGlError("glTexStorage*");
	return ref;
}

PTexture GlTexture::makeMS(Type type, Format format, const int3 &size, int num_samples,
						   bool fixed_sample_locations) {
	DASSERT(isOneOf(type, Type::tex_2d_ms, Type::array_2d_ms));
	DASSERT(num_samples >= 1 && num_samples <= gl_info->limits[GlLimit::max_samples]);
	// TODO: clamp num_samples to limit ?
	PTexture ref(storage.make(size, 1, format, type));
	ref->bind();
	auto gl_type = ref->glType();
	if(type == Type::tex_2d_ms)
		glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_samples, glInternalFormat(format),
								  size.x, size.y, fixed_sample_locations);
	else if(type == Type::array_2d_ms)
		glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, num_samples,
								  glInternalFormat(format), size.x, size.y, size.z,
								  fixed_sample_locations);
	testGlError("glTexStorage*Multisample");
	return ref;
}

// TODO: make sure that it works properly in all contexts
PTexture GlTexture::makeView(Format format, PTexture view_source) {
	DASSERT(view_source);
	int3 size(view_source->size(), view_source->m_depth);
	PTexture ref(storage.make(size, view_source->m_num_levels, format, view_source->type()));
	glTextureView(ref.id(), ref->glType(), view_source.id(), glInternalFormat(format), 0,
				  view_source->m_num_levels, 0, size.z);
	return ref;
}

PTexture GlTexture::make(const Image &image, Format format) { return make(cspan({image}), format); }

PTexture GlTexture::make(CSpan<Image> levels, Format format) {
	DASSERT(levels);
	auto tex = make(Type::tex_2d, format, {levels[0].size(), 1}, levels.size());
	for(int i = 0; i < levels.size(); i++)
		tex->upload(levels[i], i);
	return tex;
}

PTexture GlTexture::make(CSpan<FloatImage> levels, Format format) {
	DASSERT(levels);
	auto tex = make(Type::tex_2d, format, {levels[0].size(), 1}, levels.size());
	for(int i = 0; i < levels.size(); i++)
		tex->upload(levels[i], i);
	return tex;
}

PTexture GlTexture::make(CSpan<CompressedImage> levels, Maybe<Format> format) {
	DASSERT(levels);
	if(!format)
		format = levels[0].format();
	auto tex = make(Type::tex_2d, *format, {levels[0].size(), 1}, levels.size());
	for(int i = 0; i < levels.size(); i++)
		tex->upload(levels[i], i);
	return tex;
}

int GlTexture::estimateGpuMemory() const {
	// TODO: buffered & view does not take any additional memory
	// TODO: mipmaps & layers
	return imageSize(m_format, m_width, m_height);
}

void GlTexture::setFiltering(const TextureFilteringParams &params) {
	uint mag_filter = params.magnification == FilterOpt::nearest ? GL_NEAREST : GL_LINEAR;
	uint min_filter;
	if(params.mipmap && m_num_levels > 1) {
		min_filter = params.minification == FilterOpt::nearest
						 ? (params.mipmap == FilterOpt::nearest ? GL_NEAREST_MIPMAP_NEAREST
																: GL_NEAREST_MIPMAP_LINEAR)
						 : (params.mipmap == FilterOpt::nearest ? GL_LINEAR_MIPMAP_NEAREST
																: GL_LINEAR_MIPMAP_LINEAR);
	} else {
		min_filter = params.minification == FilterOpt::nearest ? GL_NEAREST : GL_LINEAR;
	}

	bind();
	auto gl_type = glType();
	glTexParameteri(gl_type, GL_TEXTURE_MAG_FILTER, mag_filter);
	glTexParameteri(gl_type, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameterf(gl_type, GL_TEXTURE_MAX_ANISOTROPY, params.max_anisotropy_samples);
}

void GlTexture::setFiltering(TextureFilterOpt filter, int max_aniso_samples) {
	DASSERT(max_aniso_samples >= 0);
	setFiltering({filter, filter, filter, u8(max_aniso_samples)});
}

void GlTexture::setWrapping(WrapOpt u_wrap, WrapOpt v_wrap, WrapOpt w_wrap) {
	DASSERT(!isMultisampled());
	bind();
	glTexParameteri(glType(), GL_TEXTURE_WRAP_S, wrap_ids[u_wrap]);
	glTexParameteri(glType(), GL_TEXTURE_WRAP_T, wrap_ids[v_wrap]);
	glTexParameteri(glType(), GL_TEXTURE_WRAP_R, wrap_ids[w_wrap]);
}

TextureFilteringParams GlTexture::filtering() const {
	TextureFilteringParams out;
	bind();
	GLint mag_filter, min_filter;
	float max_aniso;
	glGetTexParameteriv(glType(), GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(glType(), GL_TEXTURE_MAG_FILTER, &mag_filter);
	out.magnification = mag_filter == GL_NEAREST ? FilterOpt::nearest : FilterOpt::linear;
	out.minification =
		isOneOf(min_filter, GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR)
			? FilterOpt::nearest
			: FilterOpt::linear;
	if(isOneOf(min_filter, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST))
		out.mipmap = FilterOpt::nearest;
	if(isOneOf(min_filter, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR))
		out.mipmap = FilterOpt::linear;
	glGetTexParameterfv(glType(), GL_TEXTURE_MAX_ANISOTROPY, &max_aniso);
	out.max_anisotropy_samples = u8(max(roundf(max_aniso), 1.0f));
	return out;
}

array<WrapOpt, 3> GlTexture::wrapping() const {
	array<WrapOpt, 3> out;
	bind();

	GLint values[3];
	glGetTexParameteriv(glType(), GL_TEXTURE_WRAP_S, values + 0);
	glGetTexParameteriv(glType(), GL_TEXTURE_WRAP_T, values + 1);
	glGetTexParameteriv(glType(), GL_TEXTURE_WRAP_R, values + 2);

	for(int i = 0; i < 3; i++) {
		WrapOpt value = WrapOpt::repeat;
		for(WrapOpt opt : all<WrapOpt>)
			if(wrap_ids[opt] == values[i]) {
				value = opt;
				break;
			}
	}

	return out;
}

void GlTexture::setSwizzle(CSpan<int, 4> component_swizzle) {
	const int params[4] = {GL_TEXTURE_SWIZZLE_R, GL_TEXTURE_SWIZZLE_G, GL_TEXTURE_SWIZZLE_B,
						   GL_TEXTURE_SWIZZLE_A};
	const int values[4] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
	for(int i = 0; i < 4; i++) {
		int swizzle = component_swizzle[i];
		DASSERT(swizzle >= 0 && swizzle < 4);
		glTexParameteri(glType(), params[i], values[swizzle]);
	}
}

void GlTexture::generateMipmaps() {
	DASSERT(!isOneOf(m_type, Type::buffer_1d, Type::array_2d_ms, Type::tex_2d_ms));
	bind();
	glGenerateMipmap(glType());
}

// TODO: uploading of 3D textures & arrays
void GlTexture::upload(Format format, CSpan<u8> data, const IRect &rect, int mipmap_level) {
	bind();
	DASSERT(mipmap_level >= 0 && mipmap_level < m_num_levels);
	auto mip_size = size(mipmap_level);
	DASSERT(rect.x() >= 0 && rect.y() >= 0);
	DASSERT(rect.ex() <= mip_size.x && rect.ey() <= mip_size.y);

	if(isCompressed(m_format)) {
		// TODO: more restrictions
		DASSERT(compressedBlockSize(m_format) == compressedBlockSize(format));
		DASSERT(rect.x() % 4 == 0 && rect.y() % 4 == 0);
	} else {
		// TODO: check for format compatibility
		// TODO: make sure that we have enough data in pixels.data()
	}

	if(isCompressed(m_format)) {
		glCompressedTexSubImage2D(glType(), mipmap_level, rect.x(), rect.y(), rect.width(),
								  rect.height(), glInternalFormat(m_format), data.size(),
								  data.data());
	} else {
		auto [pformat, ptype] = glPackedFormat(format);
		glTexSubImage2D(glType(), mipmap_level, rect.x(), rect.y(), rect.width(), rect.height(),
						pformat, ptype, data.data());
	}
}

// TODO: add ImageData class which all Image types can convert to (and select sub-regions as well?)
void GlTexture::upload(const Image &src, int mipmap_level) {
	upload(src.format(), src.data().reinterpret<u8>(), IRect(src.size()), mipmap_level);
}

void GlTexture::upload(const FloatImage &src, int mipmap_level) {
	upload(src.format(), src.data().reinterpret<u8>(), IRect(src.size()), mipmap_level);
}

void GlTexture::upload(const CompressedImage &src, int mipmap_level) {
	upload(src.format(), src.data().reinterpret<u8>(), IRect(src.size()), mipmap_level);
}

void GlTexture::download(Image &target, int mipmap_level) const {
	bind();
	target.resize(size(mipmap_level));
	auto [pformat, ptype] = glPackedFormat(GlFormat::rgba8);
	glGetTexImage(glType(), mipmap_level, pformat, ptype, target.data().data());
}

void GlTexture::download(FloatImage &target, int mipmap_level) const {
	bind();
	target.resize(size(mipmap_level));
	auto [pformat, ptype] = glPackedFormat(GlFormat::rgba32f);
	glGetTexImage(glType(), mipmap_level, pformat, ptype, target.data().data());
}

// TODO: change to blit (just like in Image) do the same in framebuffer?
void GlTexture::copyTo(PTexture dst, IRect src_rect, int2 dst_pos) const {
	DASSERT(dst);
	DASSERT(contains(src_rect));
	IRect dst_rect(dst_pos, dst_pos + src_rect.size());
	DASSERT(dst->contains(dst_rect));

	glCopyImageSubData(id(), glType(), 0, src_rect.x(), src_rect.y(), 0, dst.id(), dst->glType(), 0,
					   dst_pos.x, dst_pos.y, 0, src_rect.width(), src_rect.height(), 1);
}

void GlTexture::bind() const {
	PASSERT_GL_THREAD();
	glActiveTexture(GL_TEXTURE0);
	::glBindTexture(glType(), id());
}

void GlTexture::bind(int index) const {
	PASSERT_GL_THREAD();
	glActiveTexture(GL_TEXTURE0 + index);
	::glBindTexture(glType(), id());
}

void GlTexture::bind(CSpan<PTexture> set, int first) {
	static int max_bind = 0;
	PASSERT_GL_THREAD();

	GLuint indices[256];
	for(int i = 0; i < set.size(); i += arraySize(indices)) {
		int count = min(set.size() - i, arraySize(indices));
		for(int j = 0; j < count; j++) {
			auto &tex = set[i + j];
			indices[j] = tex ? tex.id() : 0;
		}
		glBindTextures(first + i, count, indices);
	}
}

void GlTexture::clear(float4 value, int mipmap_level) {
	auto [format, type] = glPackedFormat(GlFormat::rgba32f);
	glClearTexImage(id(), mipmap_level, format, type, &value);
	testGlError("glClearTexImage");
}

void GlTexture::clear(CSpan<char> value, Format format, int mipmap_level) {
	DASSERT(value.size() == bytesPerPixel(format));
	auto [pformat, ptype] = glPackedFormat(format);
	glClearTexImage(id(), mipmap_level, pformat, ptype, value.data());
	testGlError("glClearTexImage");
}

void GlTexture::clear(IColor value, int mipmap_level) { clear(FColor(value), mipmap_level); }

int2 GlTexture::size(int mipmap_level) const {
	if(mipmap_level >= m_num_levels)
		return {};
	return {max(1, m_width >> mipmap_level), max(1, m_height >> mipmap_level)};
}

static int gl_access[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

void GlTexture::bindImage(int unit, AccessMode access, int level, Maybe<int> target_format) {
	DASSERT(unit >= 0);
	glBindImageTexture(unit, id(), level, GL_FALSE, 0, gl_access[(int)access],
					   target_format.orElse(glInternalFormat(m_format)));
}

void GlTexture::unbind() {
	PASSERT_GL_THREAD();
	// TODO: unbind multisample ?
	::glBindTexture(GL_TEXTURE_2D, 0);
}

bool GlTexture::contains(const IRect &rect) const { return IRect(size()).contains(rect); }
int GlTexture::numSamples() const {
	bind();
	GLint value = 0;
	glGetTexLevelParameteriv(glType(), 0, GL_TEXTURE_SAMPLES, &value);
	return value;
}

int GlTexture::glType() const { return type_ids[m_type]; }
}
