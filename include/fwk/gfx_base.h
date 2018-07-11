// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

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
class ShaderStorage;
class Shader;
class Program;
class ProgramBinder;
class SimpleMaterial;
class Material;
class MaterialSet;
class MatrixStack;
class Renderer2D;
class DrawCall;
class RenderList;
class FontCore;
class Font;
class FontFactory;
struct Pose;
class MeshIndices;
struct MeshBuffers;
class Mesh;
class DynamicMesh;
class Model;
class ModelNode;
class ModelAnim;

class TriangleBuffer;
class SpriteBuffer;
class LineBuffer;
template <> constexpr int type_size<TriangleBuffer> = 112;
template <> constexpr int type_size<LineBuffer> = 112;

struct FColor;
struct IColor;
enum class ColorId : unsigned char;

struct ColoredTriangle;

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
using SShaderStorage = shared_ptr<ShaderStorage>;

using PProgram = immutable_ptr<Program>;
using PFontCore = immutable_ptr<FontCore>;

using PPose = immutable_ptr<Pose>;
using PMesh = immutable_ptr<Mesh>;
using PModel = immutable_ptr<Model>;
using PModelNode = UniquePtr<ModelNode>;

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

// All OpenGL operations should be performed on the same thread
// This function makes sure that this is the case
void assertGfxThread();

// Will return false if no GfxDevice is present
bool onGfxThread();

#ifdef FWK_PARANOID
#define PASSERT_GFX_THREAD() assertGfxThread()
#else
#define PASSERT_GFX_THREAD()
#endif
}
