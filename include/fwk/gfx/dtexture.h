// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/texture_format.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"
#include "fwk/sys/immutable_ptr.h"

namespace fwk {

DEFINE_ENUM(TextureOpt, multisample, wrapped, filtered, immutable);
using TextureFlags = EnumFlags<TextureOpt>;

// Device texture
class DTexture : public immutable_base<DTexture> {
  public:
	using Format = TextureFormat;
	using Opt = TextureOpt;
	using Flags = TextureFlags;

	DTexture();
	DTexture(const string &name, Stream &);
	DTexture(Format, const int2 &size, int multisample_count, Flags = Opt::multisample);
	DTexture(Format, const int2 &size, Flags = {});
	DTexture(Format, const Texture &, Flags = {});
	DTexture(Format, const int2 &size, CSpan<float4>, Flags = {});
	DTexture(Format, const DTexture &view_source);
	DTexture(const Texture &, Flags = {});

	// TODO: think about this:
	// some opengl handle may be assigned to something, example:
	// texture assigned to FBO; At the same time FBO keeps shared_ptr to this texture
	//  DTexture(DTexture &&);

	void operator=(DTexture &&) = delete;
	DTexture(const DTexture &) = delete;
	void operator=(const DTexture &) = delete;
	~DTexture();

	void setFlags(Flags);
	Flags flags() const { return m_flags; }

	bool hasMipmaps() const { return m_has_mipmaps; }
	void generateMipmaps();

	void bind() const;

	bool hasImmutableFormat() const;

	// TODO: one overload should be enough
	static void bind(const vector<const DTexture *> &);
	static void bind(const vector<immutable_ptr<DTexture>> &);
	static void unbind();

	void bindImage(int unit, AccessMode access, int level = 0);

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

	void copy(const DTexture &source, IRect src_rect, int2 target_pos);

	void clear(float4);
	void clear(int);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }

	IRect rect() const;
	bool contains(const IRect &) const;

	Format format() const { return m_format; }

	int id() const { return m_id; }
	int glType() const;
	// bool isValid() const { return m_id > 0; }

  private:
	void updateParams();

	uint m_id;
	int2 m_size;
	Format m_format;
	Flags m_flags;
	bool m_has_mipmaps;
};
}
