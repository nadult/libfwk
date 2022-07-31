// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

class CompressedImage;
class DynamicMesh;
class FloatImage;
class Font;
class FontCore;
class FontFactory;
class GlDevice;
class Image;
class LineBuffer;
class MatrixStack;
class Mesh;
class MeshIndices;
class Model;
class ModelAnim;
class ModelNode;
class Renderer2D;
class ShaderCompiler;
class SpriteBuffer;
class TriangleBuffer;
class Visualizer2;
class Visualizer3;
class VulkanDevice;
struct MeshBuffers;
struct Pose;

struct SimpleDrawCall;
class SimpleMaterial;
class SimpleMaterialSet;

struct FColor;
struct IColor;
DECLARE_ENUM(ColorId);

struct ColoredTriangle;
struct ColoredQuad;

DECLARE_ENUM(GlFormat);

struct FramebufferTarget;
struct FontStyle;

struct FppCamera;
struct OrthoCamera;
struct OrbitingCamera;
struct PlaneCamera;
using CameraVariant = Variant<OrbitingCamera, FppCamera, PlaneCamera, OrthoCamera>;

class Camera;
struct CameraParams;
class CameraControl;

DEFINE_ENUM(AccessMode, read_only, write_only, read_write);

DEFINE_ENUM(BufferType, array, element_array, copy_read, copy_write, pixel_unpack, pixel_pack,
			query, texture, transform_fedback, uniform, draw_indirect, atomic_counter,
			dispatch_indirect, shader_storage);

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

template <> inline constexpr int type_size<TriangleBuffer> = sizeof(void *) == 8 ? 112 : 84;
template <> inline constexpr int type_size<LineBuffer> = sizeof(void *) == 8 ? 112 : 84;

template <> inline constexpr bool is_flat_data<ColoredTriangle> = true;
template <> inline constexpr bool is_flat_data<IColor> = true;
template <> inline constexpr bool is_flat_data<FColor> = true;
}
