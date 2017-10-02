// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_FORWARD_DECLS_H
#define FWK_FORWARD_DECLS_H

#include "fwk_base.h"

namespace fwk {

// Audio
class Sound;
class DSound;
class OggStream;
class AudioDevice;

// GFX:
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
class SDLKeyMap;
class InputEvent;
class InputState;
class Matrix3;
class Matrix4;
class Matrix3;
class Matrix4;
class AxisAngle;
class Quat;
class Tetrahedron;
class Projection;
class Frustum;
class Cylinder;
class Random;

struct HeightMap16bit;
struct TextureConfig;
struct RectStyle;
struct VertexDataType;
struct FrameBufferTarget;
struct FontStyle;

enum class TextureFormatId : unsigned char;
enum class GfxDeviceOpt : unsigned char;
enum class PrimitiveType : unsigned char;
enum class ShaderType : unsigned char;
enum class MaterialOpt : unsigned char;

using PProgram = immutable_ptr<Program>;
using SFrameBuffer = shared_ptr<FrameBuffer>;

// Mesh:
class MeshIndices;
class Mesh;
class DynamicMesh;
class Model;
class ModelAnim;
class ModelNode;
class ModelNode;
class RenderList;
class Model;
class AnimatedModel;

// XML:
class XMLNode;
class XMLDocument;

// Other:
class Converter;
class TextParser;
class Profiler;
}

#endif
