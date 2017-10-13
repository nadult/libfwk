// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/texture_format.h"
#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"
#include "fwk_math.h"

namespace fwk {

struct TextureConfig {
	enum Flags {
		flag_wrapped = 1u,
		flag_filtered = 2u,
	};

	TextureConfig(uint flags = 0) : flags(flags) {}
	bool operator==(const TextureConfig &rhs) const { return flags == rhs.flags; }

	uint flags;
};

// Device texture
class DTexture : public immutable_base<DTexture> {
  public:
	using Format = TextureFormat;
	using Config = TextureConfig;

	DTexture();
	DTexture(const string &name, Stream &);
	DTexture(Format format, const int2 &size, const Config & = Config());
	DTexture(Format format, const Texture &, const Config & = Config());
	DTexture(Format format, const int2 &size, CSpan<float4>, const Config & = Config());
	DTexture(const Texture &, const Config & = Config());

	// TODO: think about this:
	// some opengl handle may be assigned to something, example:
	// texture assigned to FBO; At the same time FBO keeps shared_ptr to this texture
	//  DTexture(DTexture &&);

	void operator=(DTexture &&) = delete;
	DTexture(const DTexture &) = delete;
	void operator=(const DTexture &) = delete;
	~DTexture();

	void setConfig(const Config &);
	const Config &config() const { return m_config; }

	bool hasMipmaps() const { return m_has_mipmaps; }
	void generateMipmaps();

	void bind() const;

	// TODO: one overload should be enough
	static void bind(const vector<const DTexture *> &);
	static void bind(const vector<immutable_ptr<DTexture>> &);
	static void unbind();

	void upload(const Texture &src, const int2 &target_pos = int2());
	void upload(Format, const void *pixels, const int2 &dimensions,
				const int2 &target_pos = int2());
	void download(Texture &target) const;

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }

	Format format() const { return m_format; }

	int id() const { return m_id; }
	// bool isValid() const { return m_id > 0; }

  private:
	void updateConfig();

	uint m_id;
	int2 m_size;
	Format m_format;
	Config m_config;
	bool m_has_mipmaps;
};
}
