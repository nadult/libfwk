// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/sys_base.h"

namespace fwk {

class Canvas2D;
class Canvas3D;
class BlockImage;
class DynamicMesh;
class FloatImage;
class Font;
class FontCore;
class FontFactory;
class GlDevice;
class Image;
class MatrixStack;
class Mesh;
class MeshIndices;
class Model;
class ModelAnim;
class ModelNode;
class ShaderCompiler;
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

// TODO: fix these
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

template <> inline constexpr bool is_flat_data<ColoredTriangle> = true;
template <> inline constexpr bool is_flat_data<IColor> = true;
template <> inline constexpr bool is_flat_data<FColor> = true;
}
