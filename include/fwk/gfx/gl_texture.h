// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(TextureFilterOpt, nearest, linear);
DEFINE_ENUM(TextureWrapOpt, clamp_to_edge, clamp_to_border, repeat, mirror_repeat,
			mirror_clamp_to_edge);

DEFINE_ENUM(TextureType, tex_1d, tex_2d, tex_3d, buffer_1d, array_1d, array_2d, tex_2d_ms,
			array_2d_ms);

struct TextureFilteringParams {
	using Opt = TextureFilterOpt;
	Opt magnification = Opt::nearest;
	Opt minification = Opt::nearest;
	Maybe<Opt> mipmap;
	u8 max_anisotropy_samples = 1;
};

// Wrapper class for OpenGL texture; All textures have immutable format
class GlTexture {
	GL_CLASS_DECL(GlTexture)
  public:
	using Format = GlFormat;
	using Type = TextureType;
	using FilterOpt = TextureFilterOpt;
	using WrapOpt = TextureWrapOpt;

	static int maxMipmapLevels(int max_dimension) { return int(log2(max_dimension)) + 1; }
	static int maxMipmapLevels(int2 size) { return maxMipmapLevels(max(size.x, size.y)); }

	static Ex<PTexture> load(ZStr path, bool generate_mipmaps);
	static PTexture make(Type, Format, const int3 &size, int num_levels);
	static PTexture makeMS(Type, Format, const int3 &size, int num_samples,
						   bool fixed_sample_locations);
	static PTexture makeView(Format, PTexture view_source);

	static PTexture make(Format format, const int2 &size, int num_levels) {
		return make(Type::tex_2d, format, {size, 1}, num_levels);
	}

	static PTexture make(const Image &, Format = Format::rgba8);
	static PTexture make(CSpan<Image>, Format = Format::rgba8);
	static PTexture make(CSpan<FloatImage>, Format = Format::rgba32f);
	static PTexture make(CSpan<CompressedImage>, Maybe<Format> override_format = none);

	void setFiltering(const TextureFilteringParams &);
	void setFiltering(TextureFilterOpt filter, int max_aniso_samples = 1);
	TextureFilteringParams filtering() const;

	void setWrapping(WrapOpt u_wrap, WrapOpt v_wrap, WrapOpt w_wrap = WrapOpt::repeat);
	void setWrapping(WrapOpt wrap) { setWrapping(wrap, wrap, wrap); }
	array<WrapOpt, 3> wrapping() const;

	void setSwizzle(CSpan<int, 4> component_swizzle);

	void generateMipmaps();

	void bind() const;
	void bind(int index) const;

	static void bind(CSpan<PTexture>, int first = 0);
	static void unbind();

	void bindImage(int unit, AccessMode access, int level = 0, Maybe<int> target_format = none);

	void upload(Format, CSpan<u8>, const IRect &rect, int mipmap_level);
	void upload(Format, PBuffer, int buffer_offset, const IRect &rect, int mipmap_level);

	void upload(CSpan<u8>, const IRect &src_rect, int2 target_pos);

	void upload(const Image &src, int mipmap_level);
	void upload(const FloatImage &src, int mipmap_level);
	void upload(const CompressedImage &src, int mipmap_level);

	void download(Image &dst, int mipmap_level = 0) const;
	void download(FloatImage &dst, int mipmap_level = 0) const;

	void copyTo(PTexture, IRect src_rect, int2 dst_pos) const;

	void clear(float4, int mipmap_level = 0);
	void clear(IColor, int mipmap_level = 0);
	template <class T> void clear(T value, Format format, int mipmap_level = 0) {
		clear(cspan(reinterpret_cast<const char *>(&value), sizeof(T)), format, mipmap_level);
	}

	int width() const { return m_width; }
	int height() const { return int(m_height); }
	int depth() const { return int(m_depth); }
	int2 size() const { return int2(m_width, m_height); }
	int2 size(int mipmap_level) const;
	bool contains(const IRect &) const;

	int estimateGpuMemory() const;

	Format format() const { return m_format; }
	Type type() const { return m_type; }
	bool isMultisampled() const { return isOneOf(m_type, Type::tex_2d_ms, Type::array_2d_ms); }
	int numMipmapLevels() const { return int(m_num_levels); }
	int numSamples() const;
	int glType() const;

  private:
	void updateParams();
	void initialize(int msaa_samples);
	GlTexture(int3 size, int num_levels, Format, Type);
	void clear(CSpan<char> value, Format, int mipmap_level);

	int m_width;
	unsigned short m_height, m_depth;
	u8 m_num_levels;
	Format m_format;
	Type m_type;
};
}
