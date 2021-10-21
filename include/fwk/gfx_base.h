// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

#if defined(FWK_CHECK_OPENGL)
#define GL_ASSERT(expr) ASSERT(expr)
#define IF_GL_CHECKS(...) __VA_ARGS__
#else
#define GL_ASSERT(expr) ((void)0)
#define IF_GL_CHECKS(...)
#endif

class Texture;
class BlockTexture;
class GlDevice;
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
template <> inline constexpr int type_size<TriangleBuffer> = sizeof(void *) == 8 ? 112 : 84;
template <> inline constexpr int type_size<LineBuffer> = sizeof(void *) == 8 ? 112 : 84;

struct FColor;
struct IColor;
DECLARE_ENUM(ColorId);

struct ColoredTriangle;
struct ColoredQuad;

DECLARE_ENUM(GlFormat);
DECLARE_ENUM(GlDeviceOpt);
DECLARE_ENUM(PrimitiveType);
DECLARE_ENUM(ShaderType);
DECLARE_ENUM(MaterialOpt);
DECLARE_ENUM(IndexType);
DECLARE_ENUM(VertexBaseType);

struct HeightMap16bit;
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

class ShaderCombiner;

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

class Visualizer2;
class Visualizer3;

struct FppCamera;
struct OrthoCamera;
struct OrbitingCamera;
struct PlaneCamera;
using CameraVariant = Variant<OrbitingCamera, FppCamera, PlaneCamera, OrthoCamera>;

class Camera;
struct CameraParams;
class CameraControl;

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

template <> inline constexpr bool is_flat_data<ColoredTriangle> = true;
template <> inline constexpr bool is_flat_data<IColor> = true;
template <> inline constexpr bool is_flat_data<FColor> = true;
}
