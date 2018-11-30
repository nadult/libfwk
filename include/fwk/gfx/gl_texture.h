// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(TextureOpt, multisample, wrapped, filtered, immutable, sample_stencil);
using TextureFlags = EnumFlags<TextureOpt>;

class GlTexture {
	GL_CLASS_DECL(GlTexture)
  public:
	using Format = GlFormat;
	using Opt = TextureOpt;
	using Flags = TextureFlags;

	// TODO: cleanup these
	static PTexture make(const string &name, Stream &);
	static PTexture make(Format, const int2 &size, int multisample_count, Flags = Opt::multisample);
	static PTexture make(Format, const int2 &size, Flags = {});
	static PTexture make(Format, const Texture &, Flags = {});
	static PTexture make(Format, const int2 &size, CSpan<float4>, Flags = {});
	static PTexture make(Format, PTexture view_source);
	static PTexture make(const Texture &, Flags = {});

	void setFlags(Flags);
	Flags flags() const { return m_flags; }

	bool hasMipmaps() const { return m_has_mipmaps; }
	void generateMipmaps();

	void bind() const;

	bool hasImmutableFormat() const;

	static void bind(CSpan<PTexture>);
	static void unbind();

	void bindImage(int unit, AccessMode access, int level = 0, Maybe<int> target_format = none);

	void upload(const Texture &src, const int2 &target_pos = int2());
	void upload(Format, const void *pixels, const int2 &dimensions,
				const int2 &target_pos = int2());
	void upload(CSpan<char>);
	template <class T> void upload(CSpan<T> data) { upload(data.template reinterpret<char>()); }

	void download(Texture &target) const;
	void download(Span<char>) const;
	template <class T> void download(Span<T> data) const {
		download(data.template reinterpret<char>());
	}

	template <class T> vector<T> download() const { return download<T>(usedMemory() / sizeof(T)); }

	template <class T> vector<T> download(int count) const {
		DASSERT(count >= 0 && count <= int(usedMemory() / sizeof(T)));
		PodVector<T> out(count);
		download<T>(out);
		vector<T> vout;
		out.unsafeSwap(vout);
		return vout;
	}

	void copyTo(PTexture, IRect src_rect, int2 dst_pos) const;

	void clear(float4);
	void clear(int);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int usedMemory() const;

	IRect rect() const;
	bool contains(const IRect &) const;

	Format format() const { return m_format; }

	int glType() const;

  private:
	void updateParams();
	void initialize(int msaa_samples);
	GlTexture(Format, int2, Flags);

	int2 m_size;
	Format m_format;
	Flags m_flags;
	bool m_has_mipmaps = false;
	bool m_stencil_sample = false;
};
}
