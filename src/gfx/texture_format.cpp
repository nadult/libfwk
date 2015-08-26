/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <GL/gl.h>
#include <GL/glext.h>

namespace fwk {

DEFINE_ENUM(TextureFormatId, "rgba", "rgba_f16", "rgba_f32", "luminance", "dxt1", "dxt3", "dxt5",
			"depth", "depth_stencil");

enum class DDSId {
	Unknown = 0,

	R8G8B8 = 20,
	A8R8G8B8 = 21,
	X8R8G8B8 = 22,
	R5G6B5 = 23,
	X1R5G5B5 = 24,
	A1R5G5B5 = 25,
	A4R4G4B4 = 26,
	R3G3B2 = 27,
	A8 = 28,
	A8R3G3B2 = 29,
	X4R4G4B4 = 30,
	A2B10G10R10 = 31,
	A8B8G8R8 = 32,
	X8B8G8R8 = 33,
	G16R16 = 34,
	A2R10G10B10 = 35,
	A16B16G16R16 = 36,

	L8 = 50,
	A8L8 = 51,
	A4L4 = 52,

	V8U8 = 60,
	L6V5U5 = 61,
	X8L8V8U8 = 62,
	Q8W8V8U8 = 63,
	V16U16 = 64,
	A2W10V10U10 = 67,

	UYVY = 0x59565955,		// MAKEFOURCC('U', 'Y', 'V', 'Y'),
	R8G8_B8G8 = 0x47424752, // MAKEFOURCC('R', 'G', 'B', 'G'),
	YUY2 = 0x32595559,		// MAKEFOURCC('Y', 'U', 'Y', '2'),
	G8R8_G8B8 = 0x42475247, // MAKEFOURCC('G', 'R', 'G', 'B'),
	DXT1 = 0x31545844,		// MAKEFOURCC('D', 'X', 'T', '1'),
	DXT2 = 0x32545844,		// MAKEFOURCC('D', 'X', 'T', '2'),
	DXT3 = 0x33545844,		// MAKEFOURCC('D', 'X', 'T', '3'),
	DXT4 = 0x34545844,		// MAKEFOURCC('D', 'X', 'T', '4'),
	DXT5 = 0x35545844,		// MAKEFOURCC('D', 'X', 'T', '5'),

	L16 = 81,
	Q16W16V16U16 = 110,

	// Floating point surface formats
	// s10e5 formats (16-bits per channel)
	R16F = 111,
	G16R16F = 112,
	A16B16G16R16F = 113,

	// IEEE s23e8 formats (32-bits per channel)
	R32F = 114,
	G32R32F = 115,
	A32B32G32R32F = 116,
};

using Id = TextureFormatId::Type;

namespace {
	struct FormatDesc {
		FormatDesc(Id id, DDSId dds_id, int bpp, int i, int f, int t, bool is_compressed = false)
			: id(id), dds_id(dds_id), bytes_per_pixel(bpp), internal(i), gFormat(f), type(t),
			  is_compressed(is_compressed) {}

		Id id;
		DDSId dds_id;
		int bytes_per_pixel;
		int internal, gFormat, type;
		bool is_compressed;
	};

	const FormatDesc s_descs[] = {
		FormatDesc(Id::rgba, DDSId::A8B8G8R8, 4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE),
		FormatDesc(Id::rgba_f16, DDSId::A16B16G16R16F, 8, GL_RGBA16F, GL_RGBA, GL_FLOAT),
		FormatDesc(Id::rgba_f32, DDSId::A32B32G32R32F, 16, GL_RGBA32F, GL_RGBA, GL_FLOAT),
		FormatDesc(Id::luminance, DDSId::L8, 1, GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE),

		FormatDesc(Id::dxt1, DDSId::DXT1, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, true),
		FormatDesc(Id::dxt3, DDSId::DXT3, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, true),
		FormatDesc(Id::dxt5, DDSId::DXT5, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, true),

		FormatDesc(Id::depth, DDSId::Unknown, 4, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
				   GL_UNSIGNED_SHORT), // TODO: WebGL requires 16bits
		FormatDesc(Id::depth_stencil, DDSId::Unknown, 4, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
				   GL_UNSIGNED_INT_24_8),

	};

	const FormatDesc &getDesc(Id id) {
		DASSERT(id >= 0 && id < arraySize(s_descs));
		DASSERT(s_descs[id].id == id);
		return s_descs[id];
	}
}

int TextureFormat::glInternal() const { return getDesc(m_id).internal; }
int TextureFormat::glFormat() const { return getDesc(m_id).gFormat; }
int TextureFormat::glType() const { return getDesc(m_id).type; }
bool TextureFormat::isCompressed() const { return getDesc(m_id).is_compressed; }
int TextureFormat::bytesPerPixel() const { return getDesc(m_id).bytes_per_pixel; }

bool TextureFormat::isSupported() const {
	if(m_id == Id::dxt1)
		return false; // glExtAvaliable(OE_EXT_TEXTURE_COMPRESSION_S3TC)||glExtAvaliable(OE_EXT_TEXTURE_COMPRESSION_DXT1);
	if(isOneOf(m_id, Id::dxt3, Id::dxt5))
		return false; // glExtAvaliable(OE_EXT_TEXTURE_COMPRESSION_S3TC);

	return true;
}

int TextureFormat::evalImageSize(int width, int height) const {
	if(m_id == Id::dxt1)
		return ((width + 3) / 4) * ((height + 3) / 4) * 8;
	if(isOneOf(m_id, Id::dxt3, Id::dxt5))
		return ((width + 3) / 4) * ((height + 3) / 4) * 16;

	return width * height * getDesc(m_id).bytes_per_pixel;
}

int TextureFormat::evalLineSize(int width) const {
	if(m_id == Id::dxt1)
		return ((width + 3) / 4) * 8;
	if(isOneOf(m_id, Id::dxt3, Id::dxt5))
		return ((width + 3) / 4) * 16;

	return width * getDesc(m_id).bytes_per_pixel;
}
}
