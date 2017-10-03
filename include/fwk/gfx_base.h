// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_base.h"

namespace fwk {

class TextureFormat;
class Texture;
class DTexture;
class GfxDevice;
class VertexBuffer;
class IndexBuffer;
class VertexArraySource;
class VertexArray;
class RenderBuffer;
class FrameBuffer;
class Shader;
class Program;
class ProgramBinder;
class SimpleMaterial;
class Material;
class MaterialSet;
class MatrixStack;
class Renderer2D;
class SpriteBuffer;
class LineBuffer;
class DrawCall;
class RenderList;
class FontCore;
class Font;
class FontFactory;

struct FColor;
struct IColor;
enum class ColorId : unsigned char;

enum class TextureFormatId : unsigned char;
enum class GfxDeviceOpt : unsigned char;
enum class PrimitiveType : unsigned char;
enum class ShaderType : unsigned char;
enum class MaterialOpt : unsigned char;

struct HeightMap16bit;
struct TextureConfig;
struct VertexDataType;
struct FrameBufferTarget;
struct FontStyle;

using PTexture = immutable_ptr<DTexture>;
using STexture = shared_ptr<DTexture>;

using PVertexBuffer = immutable_ptr<VertexBuffer>;
using PIndexBuffer = immutable_ptr<IndexBuffer>;

using PVertexArray = immutable_ptr<VertexArray>;
using SRenderBuffer = shared_ptr<RenderBuffer>;
using SFrameBuffer = shared_ptr<FrameBuffer>;

using PProgram = immutable_ptr<Program>;
using PFontCore = immutable_ptr<FontCore>;

DEFINE_ENUM(PrimitiveType, points, lines, triangles, triangle_strip);

enum class HAlign {
	left,
	center,
	right,
};

enum class VAlign {
	top,
	center,
	bottom,
};
}