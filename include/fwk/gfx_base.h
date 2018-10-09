// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

class TextureFormat;
class Texture;
class GlDevice;
class ProgramBinder;
class SimpleMaterial;
class Material;
class MaterialSet;
class MatrixStack;
class Renderer2D;
class DrawCall;
class MultiDrawCall;
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
class Buffer;

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
enum class GlDeviceOpt : unsigned char;
enum class PrimitiveType : unsigned char;
enum class ShaderType : unsigned char;
enum class MaterialOpt : unsigned char;
enum class IndexType : unsigned char;
enum class VertexBaseType : unsigned char;

struct HeightMap16bit;
struct TextureConfig;
struct VertexDataType;
struct FramebufferTarget;
struct FontStyle;

// All of these are based on GlStorage:
class GlBuffer;
class GlFramebuffer;
class GlProgram;
class GlProgramPipeline;
class GlQuery;
class GlRenderbuffer;
class GlSampler;
class GlShader;
class GlTexture;
class GlVertexArray;
class GlTransformFeedback;

template <class T> class GlStorage;
template <class T> class GlRef;

using PBuffer = GlRef<GlBuffer>;
using PVertexArray = GlRef<GlVertexArray>;
using PProgram = GlRef<GlProgram>;
using PShader = GlRef<GlShader>;
using PTexture = GlRef<GlTexture>;
using PRenderbuffer = GlRef<GlRenderbuffer>;
using PFramebuffer = GlRef<GlFramebuffer>;
using PQuery = GlRef<GlQuery>;
// TODO: more definitions
// TODO: consistent naming

using PFontCore = immutable_ptr<FontCore>;
using PPose = immutable_ptr<Pose>;
using PMesh = immutable_ptr<Mesh>;
using PModel = immutable_ptr<Model>;
using PModelNode = UniquePtr<ModelNode>;

DEFINE_ENUM(PrimitiveType, points, lines, triangles, triangle_strip);

DEFINE_ENUM(AccessMode, read_only, write_only, read_write);

DEFINE_ENUM(BufferType, array, element_array, copy_read, copy_write, pixel_unpack, pixel_pack,
			query, texture, transform_fedback, uniform, draw_indirect, atomic_counter,
			dispatch_indirect, shader_storage);

DEFINE_ENUM(GlProfile, core, compatibility, es);

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
void assertGlThread();

// Will return false if no GlDevice is present
bool onGlThread();

#ifdef FWK_PARANOID
#define PASSERT_GL_THREAD() assertGlThread()
#else
#define PASSERT_GL_THREAD()
#endif
}
