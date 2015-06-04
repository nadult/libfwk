/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_GFX_H
#define FWK_GFX_H

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_input.h"
#include "fwk_xml.h"

class aiScene;

namespace fwk {
namespace collada {
	class Root;
	class Mesh;
}

class MakeRect {};
class MakeBBox {};

struct Color {
	explicit Color(u8 r, u8 g, u8 b, u8 a = 255) : r(r), g(g), b(b), a(a) {}
	explicit Color(int r, int g, int b, int a = 255)
		: r(clamp(r, 0, 255)), g(clamp(g, 0, 255)), b(clamp(b, 0, 255)), a(clamp(a, 0, 255)) {}
	explicit Color(float r, float g, float b, float a = 1.0f)
		: r(clamp(r * 255.0f, 0.0f, 255.0f)), g(clamp(g * 255.0f, 0.0f, 255.0f)),
		  b(clamp(b * 255.0f, 0.0f, 255.0f)), a(clamp(a * 255.0f, 0.0f, 255.0f)) {}
	explicit Color(const float3 &c, float a = 1.0f) : Color(c.x, c.y, c.z, a) {}
	explicit Color(const float4 &c) : Color(c.x, c.y, c.z, c.w) {}
	Color(Color col, u8 alpha) : r(col.r), g(col.g), b(col.b), a(alpha) {}
	Color() = default;

	Color operator|(Color rhs) const { return Color(r | rhs.r, g | rhs.g, b | rhs.b, a | rhs.a); }
	operator float4() const { return float4(r, g, b, a) * (1.0f / 255.0f); }
	operator float3() const { return float3(r, g, b) * (1.0f / 255.0f); }

	static const Color white, gray, yellow, red, green, blue, black, transparent;
	bool operator<(const Color &rhs) const;

	union {
		struct {
			u8 r, g, b, a;
		};
		u8 rgba[4];
	};
};

Color operator*(Color lhs, Color rhs);
Color mulAlpha(Color color, float alpha);
Color lerp(Color a, Color b, float value);
Color desaturate(Color col, float value);

inline bool operator==(const Color &lhs, const Color &rhs) {
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline Color swapBR(Color col) {
	swap(col.b, col.r);
	return col;
}

enum TextureIdent {
	TI_Unknown = 0,

	TI_R8G8B8 = 20,
	TI_A8R8G8B8 = 21,
	TI_X8R8G8B8 = 22,
	TI_R5G6B5 = 23,
	TI_X1R5G5B5 = 24,
	TI_A1R5G5B5 = 25,
	TI_A4R4G4B4 = 26,
	TI_R3G3B2 = 27,
	TI_A8 = 28,
	TI_A8R3G3B2 = 29,
	TI_X4R4G4B4 = 30,
	TI_A2B10G10R10 = 31,
	TI_A8B8G8R8 = 32,
	TI_X8B8G8R8 = 33,
	TI_G16R16 = 34,
	TI_A2R10G10B10 = 35,
	TI_A16B16G16R16 = 36,

	TI_L8 = 50,
	TI_A8L8 = 51,
	TI_A4L4 = 52,

	TI_V8U8 = 60,
	TI_L6V5U5 = 61,
	TI_X8L8V8U8 = 62,
	TI_Q8W8V8U8 = 63,
	TI_V16U16 = 64,
	TI_A2W10V10U10 = 67,

	TI_UYVY = 0x59565955,	  // MAKEFOURCC('U', 'Y', 'V', 'Y'),
	TI_R8G8_B8G8 = 0x47424752, // MAKEFOURCC('R', 'G', 'B', 'G'),
	TI_YUY2 = 0x32595559,	  // MAKEFOURCC('Y', 'U', 'Y', '2'),
	TI_G8R8_G8B8 = 0x42475247, // MAKEFOURCC('G', 'R', 'G', 'B'),
	TI_DXT1 = 0x31545844,	  // MAKEFOURCC('D', 'X', 'T', '1'),
	TI_DXT2 = 0x32545844,	  // MAKEFOURCC('D', 'X', 'T', '2'),
	TI_DXT3 = 0x33545844,	  // MAKEFOURCC('D', 'X', 'T', '3'),
	TI_DXT4 = 0x34545844,	  // MAKEFOURCC('D', 'X', 'T', '4'),
	TI_DXT5 = 0x35545844,	  // MAKEFOURCC('D', 'X', 'T', '5'),

	TI_L16 = 81,
	TI_Q16W16V16U16 = 110,

	// Floating point surface formats
	// s10e5 formats (16-bits per channel)
	TI_R16F = 111,
	TI_G16R16F = 112,
	TI_A16B16G16R16F = 113,

	// IEEE s23e8 formats (32-bits per channel)
	TI_R32F = 114,
	TI_G32R32F = 115,
	TI_A32B32G32R32F = 116,

	TI_FORCE_DWORD = 0x7fffffff
};

class TextureFormat {
  public:
	TextureFormat(int internal, int format, int type, bool compressed = 0);
	TextureFormat(int internal);
	TextureFormat(TextureIdent fmt);
	TextureFormat();

	TextureIdent ident() const;
	int glInternal() const;
	int glFormat() const;
	int glType() const;
	bool isCompressed() const;

	int bytesPerPixel() const;

	int evalImageSize(int width, int height) const;
	int evalLineSize(int width) const;

	bool operator==(const TextureFormat &) const;
	bool isSupported() const;

  private:
	unsigned id;
};

// simple RGBA32 texture
class Texture {
  public:
	Texture(int width, int height);
	Texture();

	// data is not preserved
	void resize(int width, int height);
	void clear();
	void fill(Color);
	void blit(Texture const &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool isEmpty() const { return m_data.isEmpty(); }
	bool testPixelAlpha(const int2 &) const;

	TextureFormat format() const { return TextureFormat(TI_A8B8G8R8); }

	// Loading from TGA, BMP, PNG, DDS
	void load(Stream &);
	void save(Stream &) const;
	void swap(Texture &);

	using Loader = void (*)(Stream &, PodArray<Color> &out_data, int2 &out_size);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	void saveTGA(Stream &) const;

	Color *data() { return m_data.data(); }
	const Color *data() const { return m_data.data(); }

	Color *line(int y) {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}
	const Color *line(int y) const {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}

	Color &operator()(int x, int y) { return m_data[x + y * m_size.x]; }
	const Color operator()(int x, int y) const { return m_data[x + y * m_size.x]; }

	Color &operator[](int idx) { return m_data[idx]; }
	const Color operator[](int idx) const { return m_data[idx]; }

  private:
	PodArray<Color> m_data;
	int2 m_size;
};

// Device texture
class DTexture {
  public:
	DTexture();
	DTexture(const string &name, Stream &);
	DTexture(DTexture &&);
	DTexture(const DTexture &) = delete;
	void operator=(const DTexture &) = delete;
	void operator=(DTexture &&);
	~DTexture();

	void load(Stream &);

	void setWrapping(bool enable);
	bool isWrapped() const { return m_is_wrapped; }

	void setFiltering(bool enable);
	bool isFiltered() const { return m_is_filtered; }

	bool hasMipmaps() const { return m_has_mipmaps; }
	void generateMipmaps();

	void bind() const;
	static void unbind();

	void resize(TextureFormat format, int width, int height);
	void clear();

	void set(const Texture &);
	void upload(const Texture &src, const int2 &target_pos = int2(0, 0));
	void upload(const void *pixels, const int2 &dimensions, const int2 &target_pos);
	void download(Texture &target) const;
	void blit(DTexture &target, const IRect &src_rect, const int2 &target_pos) const;

	int width() const { return m_width; }
	int height() const { return m_height; }
	int2 size() const { return int2(m_width, m_height); }
	int pixelCount() const { return m_width * m_height; }
	TextureFormat format() const { return m_format; }

	int id() const { return m_id; }
	bool isValid() const { return m_id > 0; }

  private:
	int m_id;
	int m_width, m_height;
	TextureFormat m_format;
	bool m_is_wrapped, m_is_filtered;
	bool m_has_mipmaps;
	mutable bool m_is_dirty;
};

typedef shared_ptr<DTexture> PTexture;

struct InputState {
	int2 mouse_pos, mouse_move;
	int mouse_wheel;
	int mouse_buttons[InputButton::count];

	using KeyStatus = pair<int, int>;
	vector<KeyStatus> keys;
};

class GfxDevice {
  public:
	static GfxDevice &instance();

	double targetFrameTime();

	// Swaps frames and synchronizes frame rate
	void tick();

	void createWindow(int2 size, bool fullscreen);
	void destroyWindow();
	void printDeviceInfo();

	bool pollEvents();

	void grabMouse(bool);
	void showCursor(bool);

	const InputState &inputState() const { return m_input_state; }
	const vector<InputEvent> &inputEvents() const { return m_input_events; }

	using MainLoopFunction = bool (*)(GfxDevice &device);

	void runMainLoop(MainLoopFunction);

	static void clearColor(Color color);
	static void clearDepth(float depth_value);

	enum BlendingMode {
		bmDisabled,
		bmNormal,
	};

	static void setBlendingMode(BlendingMode mode);
	static void setScissorRect(const IRect &rect);
	static const IRect getScissorRect();

	static void setScissorTest(bool is_enabled);

  private:
	GfxDevice();
	~GfxDevice();

	int translateToSDL(int) const;
	int translateFromSDL(int) const;

#ifdef __EMSCRIPTEN__
	static void emscriptenCallback();
#endif
	MainLoopFunction m_main_loop_function;

	bool m_is_input_state_initialized;
	InputState m_input_state;
	vector<InputEvent> m_input_events;
	std::map<int, int> m_key_map;
	std::map<int, int> m_inv_map;
	//	double m_time_pressed[InputKey::count];
	double m_last_time;
	//	double m_press_delay;
	//	int m_clock;
};

struct RectStyle {
	RectStyle(Color fill_color = Color::white, Color border_color = Color::transparent)
		: fill_color(fill_color), border_color(border_color) {}

	Color fill_color;
	Color border_color;
};

struct VertexDataType {
	enum Type {
		type_byte,
		type_ubyte,
		type_short,
		type_ushort,
		type_float,
	};

	VertexDataType(Type type, int size, bool normalize)
		: type(type), size(size), normalize(normalize) {
		DASSERT(size >= 1 && size <= 4);
	}

	template <class T> static constexpr VertexDataType make();

	const Type type;
	const int size;
	bool normalize;
};

template <class T> struct TVertexDataType : public VertexDataType {};

#define DECLARE_VERTEX_DATA(final, base, size, normalize)                                          \
	template <> struct TVertexDataType<final> : public VertexDataType {                            \
		TVertexDataType() : VertexDataType(type_##base, size, normalize) {}                        \
	};

DECLARE_VERTEX_DATA(float4, float, 4, false)
DECLARE_VERTEX_DATA(float3, float, 3, false)
DECLARE_VERTEX_DATA(float2, float, 2, false)
DECLARE_VERTEX_DATA(float, float, 1, false)
DECLARE_VERTEX_DATA(Color, ubyte, 4, true)

#undef DECLARE_VERTEX_DATA

class VertexBuffer {
  public:
	VertexBuffer(const void *data, int size, int vertex_size, VertexDataType);
	~VertexBuffer();

	void operator=(const VertexBuffer &) = delete;
	VertexBuffer(const VertexBuffer &) = delete;

	//TODO: figure a way of passing different range types here
	template <class T>
	VertexBuffer(const vector<T> &data)
		: VertexBuffer(data.data(), (int)data.size(), (int)sizeof(T), TVertexDataType<T>()) {}
	template <class T>
	VertexBuffer(const PodArray<T> &data)
		: VertexBuffer(data.data(), data.size(), (int)sizeof(T), TVertexDataType<T>()) {}

	int size() const { return m_size; }

  private:
	unsigned m_handle;
	int m_size, m_vertex_size;
	VertexDataType m_data_type;
	friend class VertexArray;
};

using PVertexBuffer = shared_ptr<VertexBuffer>;

class IndexBuffer {
  public:
	enum IndexType {
		type_uint,
		type_ubyte,
		type_ushort,
	};

	IndexBuffer(const vector<u32> &indices)
		: IndexBuffer(indices.data(), indices.size(), sizeof(indices[0]), type_uint) {}
	IndexBuffer(const vector<u16> &indices)
		: IndexBuffer(indices.data(), indices.size(), sizeof(indices[0]), type_ushort) {}
	IndexBuffer(const vector<u8> &indices)
		: IndexBuffer(indices.data(), indices.size(), sizeof(indices[0]), type_ubyte) {}
	~IndexBuffer();

	void operator=(const IndexBuffer &) = delete;
	IndexBuffer(const IndexBuffer &) = delete;

	int size() const { return m_size; }
	int indexSize() const { return m_index_size; }
	IndexType indexType() const { return m_index_type; }

  private:
	IndexBuffer(const void *data, int size, int index_size, IndexType index_type);

	unsigned m_handle;
	int m_size, m_index_size;
	IndexType m_index_type;
	friend class VertexArray;
};

using PIndexBuffer = shared_ptr<IndexBuffer>;

DECLARE_ENUM(PrimitiveType, points, lines, triangles, triangle_strip);

class VertexArraySource {
  public:
	VertexArraySource(PVertexBuffer buffer, int offset = 0);
	VertexArraySource(const float4 &value);
	VertexArraySource(const float3 &value) : VertexArraySource(float4(value, 0.0f)) {}
	VertexArraySource(const float2 &value) : VertexArraySource(float4(value, 0.0f, 0.0f)) {}
	VertexArraySource(float value) : VertexArraySource(float4(value, 0.0f, 0.0f, 0.0f)) {}
	VertexArraySource(Color color) : VertexArraySource(float4(color)) {}
	VertexArraySource(VertexArraySource &&) = default;
	VertexArraySource(const VertexArraySource &) = default;

	int maxIndex() const;

  private:
	friend class VertexArray;
	PVertexBuffer m_buffer;
	float4 m_single_value;
	int m_offset;
};

class VertexArray {
  public:
	using Source = VertexArraySource;

	VertexArray(vector<Source>, PIndexBuffer = PIndexBuffer());
	~VertexArray();

	static shared_ptr<VertexArray> make(vector<Source> sources,
										PIndexBuffer index_buffer = PIndexBuffer()) {
		return make_shared<VertexArray>(std::move(sources), std::move(index_buffer));
	}

	void operator=(const VertexArray &) = delete;
	VertexArray(const VertexArray &) = delete;

	void draw(PrimitiveType::Type, int num_vertices, int offset = 0) const;
	void draw(PrimitiveType::Type primitive_type) const { draw(primitive_type, size()); }

	int size() const { return m_size; }

  private:
	void init();
	void bind() const;
	void bindVertexBuffer(int n) const;
	static void unbind();

	vector<Source> m_sources;
	PIndexBuffer m_index_buffer;
	int m_size;
#if OPENGL_VERSION >= 0x30
	unsigned m_handle;
#endif
};

using PVertexArray = shared_ptr<VertexArray>;

class DrawCall {
  public:
	DrawCall(PVertexArray, PrimitiveType::Type, int vertex_count, int index_offset);
	void issue() const;

  private:
	PVertexArray m_vertex_array;
	PrimitiveType::Type m_primitive_type;
	int m_vertex_count, m_index_offset;
};

class Shader {
  public:
	// TODO: fix enums, also in other gfx classes
	enum Type { tVertex, tFragment };

	explicit Shader(Type);
	~Shader();

	void load(Stream &);
	void save(Stream &) const;

	Shader(Shader &&);
	Shader(const Shader &) = delete;
	void operator=(const Shader &) = delete;
	void operator=(Shader &&);

	Type getType() const;
	unsigned getHandle() const { return m_handle; }

	string getSource() const;
	void setSource(const string &str);
	void compile();

	string name;

  private:
	unsigned m_handle;
	bool m_is_compiled;
};

class Program {
  public:
	Program(const Shader &vertex, const Shader &fragment);
	~Program();
	Program(const Program &) = delete;
	void operator=(const Program &) = delete;

	void bind() const;
	static void unbind();

	unsigned handle() const { return m_handle; }
	string getInfo() const;

	void setUniform(const char *name, float);
	void setUniform(const char *name, const float *, size_t);
	void setUniform(const char *name, const Matrix4 *, size_t);

	void setUniform(const char *name, int);
	void setUniform(const char *name, const float2 &);
	void setUniform(const char *name, const float3 &);
	void setUniform(const char *name, const float4 &);
	void setUniform(const char *name, const Matrix4 &);
	int getUniformLocation(const char *name);

	void bindAttribLocation(const char *name, unsigned);
	void link();

  private:
	unsigned m_handle;
};

using PProgram = shared_ptr<Program>;

class Renderer;
class Material;

class SimpleMeshData {
  public:
	SimpleMeshData(const aiScene &scene, int mesh_id);
	SimpleMeshData(const vector<float3> &positions, const vector<float2> &tex_coords,
				   const vector<u16> &indices);

	SimpleMeshData() = default;

	SimpleMeshData(MakeRect, const FRect &xz_rect, float y);
	SimpleMeshData(MakeBBox, const FBox &bbox);

	const FBox &boundingBox() const { return m_bounding_box; }

	void transformUV(const Matrix4 &);

	int vertexCount() const { return (int)m_positions.size(); }
	const vector<float3> &positions() const { return m_positions; }
	const vector<float3> &normals() const { return m_positions; }
	const vector<float2> &texCoords() const { return m_tex_coords; }
	const vector<u16> &indices() const { return m_indices; }

	using TriIndices = array<int, 3>;

	vector<TriIndices> trisIndices() const;

	/*
		void genAdjacency();

		int faceCount() const;
		void getFace(int face, int &i1, int &i2, int &i3) const;
	*/

  protected:
	void computeBoundingBox();

	vector<float3> m_positions;
	vector<float3> m_normals;
	vector<float2> m_tex_coords;
	vector<u16> m_indices;
	PrimitiveType::Type m_primitive_type;
	FBox m_bounding_box;
	friend class SimpleMesh;
};

class SimpleMesh {
  public:
	SimpleMesh(const SimpleMeshData &, Color color = Color::white);
	void draw(Renderer &, const Material &, const Matrix4 &matrix = Matrix4::identity()) const;

  private:
	PVertexArray m_vertex_array;
	PrimitiveType::Type m_primitive_type;
};

using PSimpleMesh = shared_ptr<SimpleMesh>;

class MeshData {
  public:
	MeshData() = default;
	MeshData(const aiScene &);

	struct Node {
		string name;
		Matrix4 trans;
		int parent_id;
		vector<int> mesh_ids;
	};

	const vector<Node> &nodes() const { return m_nodes; }
	const vector<SimpleMeshData> &meshes() const { return m_meshes; }

	const FBox &boundingBox() const { return m_bounding_box; }
	int findNode(const string &name) const;

  protected:
	void computeBoundingBox();

	FBox m_bounding_box;
	vector<SimpleMeshData> m_meshes;
	vector<Node> m_nodes;
	friend class Mesh;
};

class Mesh {
  public:
	using Node = MeshData::Node;

	Mesh(const MeshData &);
	Mesh(const aiScene &);
	//	Mesh(const Vertex *verts, int count, PrimitiveType::Type type);
	virtual ~Mesh() = default;

	void draw(Renderer &, const Material &, const Matrix4 &matrix = Matrix4::identity()) const;
	void printHierarchy() const;

  protected:
	vector<Node> m_nodes;
	vector<SimpleMesh> m_meshes;
};

using PMesh = shared_ptr<Mesh>;

// TODO: add blending
using SkeletonPose = vector<Matrix4>;

class Skeleton {
  public:
	struct Trans {
		Trans() = default;
		Trans(const float3 &scale, const float3 &pos, const Quat &quat)
			: scale(scale), pos(pos), rot(quat) {}
		Trans(const Matrix4 &);
		operator const Matrix4() const;

		float3 scale;
		float3 pos;
		Quat rot;
	};

	struct Joint {
		Matrix4 trans;
		string name;
		int parent_id;
	};

	Skeleton() = default;
	Skeleton(const aiScene &);

	const Joint &operator[](int idx) const { return m_joints[idx]; }
	Joint &operator[](int idx) { return m_joints[idx]; }
	int size() const { return (int)m_joints.size(); }
	int findJoint(const string &) const;

  private:
	vector<Joint> m_joints;
};

Matrix4 lerp(const Skeleton::Trans &, const Skeleton::Trans &, float);

class SkeletalAnim {
  public:
	using Trans = Skeleton::Trans;
	SkeletalAnim(const aiScene &, int anim_id, const Skeleton &);
	SkeletonPose animateSkeleton(const Skeleton &skeleton, double anim_time) const;

	const string &name() const { return m_name; }
	float length() const { return m_length; }
	// TODO: advanced interpolation support

  protected:
	string m_name;

	struct Channel {
		string joint_name;
		int joint_id;

		struct Key {
			Trans trans;
			float time;
		};
		vector<Key> keys;
	};

	vector<Channel> m_channels;
	float m_length;
};

class SkinnedMeshData : public MeshData {
  public:
	SkinnedMeshData();
	SkinnedMeshData(const aiScene &);
	virtual ~SkinnedMeshData() = default;

	void drawSkeleton(Renderer &, const SkeletonPose &, Color) const;

	// Pass -1 to anim_id for bind position
	SkeletonPose animateSkeleton(int anim_id, double anim_pos) const;
	const SkeletalAnim &anim(int anim_id) const { return m_anims[anim_id]; }
	int animCount() const { return (int)m_anims.size(); }

	int jointCount() const { return m_skeleton.size(); }
	const Skeleton &skeleton() const { return m_skeleton; }

	void computeBoundingBox();
	FBox computeBoundingBox(const SkeletonPose &) const;
	SimpleMeshData animateMesh(int mesh_id, const SkeletonPose &) const;

	float intersect(const Segment &, const SkeletonPose &) const;

	struct VertexWeight {
		VertexWeight(float weight, int joint_id) : weight(weight), joint_id(joint_id) {}

		float weight;
		int joint_id;
	};

	struct MeshSkin {
		vector<vector<VertexWeight>> vertex_weights;
	};
	// TODO: instancing support

  protected:
	void animateVertices(int mesh_id, const SkeletonPose &, float3 *positions,
						 float3 *normals = nullptr) const;

	float3 m_bind_scale;
	Skeleton m_skeleton;
	vector<SkeletalAnim> m_anims;
	vector<MeshSkin> m_mesh_skins;
	vector<Matrix4> m_bind_matrices;
	vector<Matrix4> m_inv_bind_matrices;
};

class SkinnedMesh {
  public:
	SkinnedMesh(const aiScene &);

	void draw(Renderer &, const SkeletonPose &, const Material &,
			  const Matrix4 &matrix = Matrix4::identity()) const;
	const SkeletalAnim &anim(int anim_id) const { return m_data.anim(anim_id); }
	int animCount() const { return m_data.animCount(); }

	SkeletonPose animateSkeleton(int anim_id, double anim_pos) const {
		return m_data.animateSkeleton(anim_id, anim_pos);
	}
	const Skeleton &skeleton() const { return m_data.skeleton(); }

	FBox boundingBox(const SkeletonPose &pose) const { return m_data.computeBoundingBox(pose); }
	FBox boundingBox() const { return m_data.boundingBox(); }

	float intersect(const Segment &ray, const SkeletonPose &pose) const {
		return m_data.intersect(ray, pose);
	}
	Matrix4 nodeTrans(const string &name, const SkeletonPose &) const;
	void printHierarchy() const;

  protected:
	SkinnedMeshData m_data;
};

using PSkinnedMesh = shared_ptr<SkinnedMesh>;



class Material {
  public:
	enum Flags {
		flag_blended = 1,
		flag_two_sided = 2,
	};

	Material(PTexture texture, Color color = Color::white, uint flags = 0) : m_texture(texture), m_color(color), m_flags(flags) {}
	Material(Color color = Color::white, uint flags = 0) : m_color(color), m_flags(flags) {}

	PTexture texture() const { return m_texture; }
	Color color() const { return m_color; }
	uint flags() const { return m_flags; }

	bool operator<(const Material &rhs) const {
		return m_flags == rhs.m_flags? m_texture == rhs.m_texture ? m_color < rhs.m_color : m_texture < rhs.m_texture : m_flags < rhs.m_flags;
	}

  protected:
	PTexture m_texture;
	Color m_color;
	uint m_flags;
};

class AssimpImporter {
  public:
	AssimpImporter();
	~AssimpImporter();

	const aiScene &loadScene(Stream &stream, uint flags, const char *extension_hint = nullptr);
	const aiScene &loadScene(const char *data, int data_size, uint flags,
							 const char *extension_hint = nullptr);
	const aiScene &loadScene(const string &file_name, uint flags);
	void freeScene();

	static uint defaultFlags();

  private:
	class Impl;
	unique_ptr<Impl> m_impl;
};

template <class T> class AssimpLoader : public ResourceLoader<T> {
  public:
	AssimpLoader(const string &prefix, const string &suffix, uint flags)
		: ResourceLoader<T>(prefix, suffix), m_flags(flags) {}

	shared_ptr<T> operator()(const string &name) {
		AssimpImporter m_importer;

		// TODO: Crash when same m_importer is used for multiple loads
		const aiScene &scene = m_importer.loadScene(ResourceLoader<T>::fileName(name), m_flags);
		auto out = make_shared<T>(scene);
		m_importer.freeScene();
		return out;
	}

  protected:
	uint m_flags;
};

struct DrawElement {};

class MatrixStack {
  public:
	MatrixStack(const Matrix4 &proj_matrix = Matrix4::identity(),
				const Matrix4 &view_matrix = Matrix4::identity());

	void pushViewMatrix();
	void popViewMatrix();
	void mulViewMatrix(const Matrix4 &);
	void setViewMatrix(const Matrix4 &);
	void setProjectionMatrix(const Matrix4 &);
	const Matrix4 &viewMatrix() const { return m_view_matrix; }
	const Matrix4 &projectionMatrix() const { return m_projection_matrix; }
	const Matrix4 &fullMatrix() const;

	const Frustum frustum() const;

  private:
	vector<Matrix4> m_matrix_stack;
	Matrix4 m_projection_matrix;
	Matrix4 m_view_matrix;
	mutable Matrix4 m_full_matrix;
	mutable bool m_is_dirty;
};

class Renderer2D : public MatrixStack {
  public:
	Renderer2D(const IRect &viewport);
	// Simple 2D view has (0, 0) in top left corner of the screen
	static Matrix4 simpleProjectionMatrix(const IRect &viewport);
	static Matrix4 simpleViewMatrix(const IRect &viewport, const float2 &view_pos);

	void setViewPos(const float2 &view_pos);

	void render();

	void addFilledRect(const FRect &rect, const FRect &tex_rect, const Material &);
	void addFilledRect(const FRect &rect, const Material &material) {
		addFilledRect(rect, FRect(0, 0, 1, 1), material);
	}

	void addRect(const FRect &rect, const Material &material);

	struct Element {
		Matrix4 matrix;
		PTexture texture;
		int first_index;
		int num_indices;
		PrimitiveType::Type primitive_type;
	};

	void addQuads(const float3 *pos, const float2 *tex_coord, const Color *color, int num_quads,
				  const Material &material);
	void addLines(const float3 *pos, const Color *color, int num_lines, const Material &material);
	void addTris(const float3 *pos, const float2 *tex_coord, const Color *color, int num_tris,
				 const Material &material);

  private:
	Element &makeElement(PrimitiveType::Type, PTexture);

	vector<float3> m_positions;
	vector<float2> m_tex_coords;
	vector<Color> m_colors;
	vector<uint> m_indices;
	vector<Element> m_elements;
	IRect m_viewport;
	PProgram m_tex_program, m_flat_program;
};

class Renderer : public MatrixStack {
  public:
	Renderer(const Matrix4 &projection_matrix = Matrix4::identity());

	void render();

	void addDrawCall(const DrawCall &, const Material &,
					 const Matrix4 &matrix = Matrix4::identity());
	void addLines(Range<const float3> verts, Color color,
				  const Matrix4 &matrix = Matrix4::identity());
	void addBBoxLines(const FBox &bbox, Color color, const Matrix4 &matrix = Matrix4::identity());
	void addSprite(TRange<const float3, 4> verts, TRange<const float2, 4> tex_coords,
				   const Material &, const Matrix4 &matrix = Matrix4::identity());

	// TODO: this is useful
	// Each line is represented by two vertices
	//	void addLines(const float3 *pos, const Color *color, int num_lines, const Material
	//&material);

	// TODO: pass ElementSource class, which can be single element, vector, pod array, whatever
  protected:
	struct Instance {
		Matrix4 matrix;
		Material material;
		DrawCall draw_call;
	};

	struct SpriteInstance {
		Matrix4 matrix;
		Material material;
		array<float3, 4> verts;
		array<float2, 4> tex_coords;
	};

	struct LineInstance {
		Matrix4 matrix;
		int first, count;
	};

	vector<SpriteInstance> m_sprites;

	vector<LineInstance> m_lines;
	vector<float3> m_line_positions;
	vector<Color> m_line_colors;

	vector<Instance> m_instances;
	PProgram m_tex_program, m_flat_program;
};

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

struct FontStyle {
	FontStyle(Color color, Color shadow_color, HAlign halign = HAlign::left,
			  VAlign valign = VAlign::top);
	FontStyle(Color color, HAlign halign = HAlign::left, VAlign valign = VAlign::top);

	Color text_color;
	Color shadow_color;
	HAlign halign;
	VAlign valign;
};

class Font {
  public:
	Font(const string &name, Stream &);
	Font();

	void load(Stream &);
	void loadFromXML(const XMLDocument &);
	const string &textureName() const { return m_texture_name; }

	struct Glyph {
		int character;
		short2 tex_pos;
		short2 size;
		short2 offset;
		short x_advance;
	};

	IRect evalExtents(const char *str, bool exact = false) const;
	IRect evalExtents(const TextFormatter &fmt, bool exact = false) const {
		return evalExtents(fmt.text(), exact);
	}
	IRect evalExtents(const string &str, bool exact = false) const {
		return evalExtents(str.c_str(), exact);
	}

	int lineHeight() const { return m_line_height; }

  private:
	friend class FontFactory;
	friend class FontRenderer;

	// Returns number of quads generated
	// For every quad it generates: 4 vectors in each buffer
	int genQuads(const char *str, float3 *out_pos, float2 *out_uv, int buffer_size) const;

	// TODO: better representation? hash table maybe?
	std::map<int, Glyph> m_glyphs;
	std::map<pair<int, int>, int> m_kernings;
	string m_texture_name;
	int2 m_texture_size;

	string m_face_name;
	IRect m_max_rect;
	int m_line_height;
};

using PFont = shared_ptr<Font>;

class FontRenderer {
  public:
	// TODO: generate vectors with coords, or generate draw calls
	FontRenderer(PFont font, PTexture texture, Renderer2D &out);

	FRect draw(const FRect &rect, const FontStyle &style, const char *text) const;
	FRect draw(const FRect &rect, const FontStyle &style, const TextFormatter &fmt) const {
		return draw(rect, style, fmt.text());
	}
	FRect draw(const FRect &rect, const FontStyle &style, string const &str) const {
		return draw(rect, style, str.c_str());
	}

	FRect draw(const float2 &pos, const FontStyle &style, const char *text) const {
		return draw(FRect(pos, pos), style, text);
	}
	FRect draw(const float2 &pos, const FontStyle &style, TextFormatter const &fmt) const {
		return draw(FRect(pos, pos), style, fmt.text());
	}
	FRect draw(const float2 &pos, const FontStyle &style, string const &str) const {
		return draw(FRect(pos, pos), style, str.c_str());
	}
	const Font &font() const { return *m_font; }

  private:
	void draw(const int2 &pos, Color col, const char *text) const;

	PFont m_font;
	PTexture m_texture;
	Renderer2D &m_renderer;
};

class FontFactory {
  public:
	FontFactory();
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	pair<PFont, PTexture> makeFont(string const &path, int size_in_pixels, bool lcd_mode = false);

  private:
	class Impl;
	unique_ptr<Impl> m_impl;
};
}

SERIALIZE_AS_POD(Color)

#endif
