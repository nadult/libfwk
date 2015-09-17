/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_GFX_H
#define FWK_GFX_H

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_input.h"
#include "fwk_xml.h"

namespace fwk {
namespace collada {
	class Root;
	class Mesh;
}

// TODO: Default color class should be float based; Change
//      u8-based Color to Color32bit or something
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

float3 SRGBToLinear(const float3 &);
float3 linearToSRGB(const float3 &);

inline bool operator==(const Color &lhs, const Color &rhs) {
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline Color swapBR(Color col) {
	swap(col.b, col.r);
	return col;
}

DECLARE_ENUM(TextureFormatId, rgba, rgba_f16, rgba_f32, rgb, rgb_f16, rgb_f32, luminance, dxt1,
			 dxt3, dxt5, depth, depth_stencil);

class TextureFormat {
  public:
	using Id = TextureFormatId::Type;

	TextureFormat(int internal, int format, int type);
	TextureFormat(Id id = Id::rgba) : m_id(id) { DASSERT(TextureFormatId::isValid(id)); }

	Id id() const { return m_id; }
	int glInternal() const;
	int glFormat() const;
	int glType() const;
	bool isCompressed() const;

	int bytesPerPixel() const;

	int evalImageSize(int width, int height) const;
	int evalLineSize(int width) const;

	bool isSupported() const;

	bool operator==(const TextureFormat &rhs) const { return m_id == rhs.m_id; }

  private:
	Id m_id;
};

struct HeightMap16bit {
  public:
	void load(Stream &);

	vector<u16> data;
	int2 size;
};

// simple RGBA32 texture
class Texture {
  public:
	Texture(int width, int height);
	Texture(Stream &);
	Texture();

	// data is not preserved
	void resize(int width, int height);
	void clear();
	void fill(Color);
	void blit(const Texture &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool isEmpty() const { return m_data.isEmpty(); }
	bool testPixelAlpha(const int2 &) const;

	TextureFormat format() const { return TextureFormatId::rgba; }

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

struct TextureConfig {
	enum Flags {
		flag_wrapped = 1u,
		flag_filtered = 2u,
	};

	TextureConfig(uint flags = 0) : flags(flags) {}
	bool operator==(const TextureConfig &rhs) const { return flags == rhs.flags; }

	uint flags;
};

//

// Device texture
class DTexture : public immutable_base<DTexture> {
  public:
	using Format = TextureFormat;
	using Config = TextureConfig;

	DTexture();
	DTexture(const string &name, Stream &);
	DTexture(Format format, const int2 &size, const Config & = Config());
	DTexture(Format format, const Texture &, const Config & = Config());
	DTexture(Format format, const int2 &size, CRange<float4>, const Config & = Config());
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

using PTexture = immutable_ptr<DTexture>;
using STexture = shared_ptr<DTexture>;

struct InputState {
	int2 mouse_pos, mouse_move;
	int mouse_wheel;
	int mouse_buttons[InputButton::count];

	using KeyStatus = pair<int, int>;
	vector<KeyStatus> keys;
};

// Once created (by calling instance first time, it will exist
// till the end of the application).
class GfxDevice {
  public:
	GfxDevice();
	~GfxDevice();

	static GfxDevice &instance();

	enum {
		flag_multisampling = 1u,
		flag_fullscreen = 2u,
		flag_fullscreen_desktop = 4u,
		flag_resizable = 8u,
		flag_centered = 16u,
		flag_vsync = 32u,
	};

	void createWindow(const string &name, const int2 &size, uint flags);
	void destroyWindow();
	void printDeviceInfo();

	void setWindowSize(const int2 &);
	int2 windowSize() const;

	void setWindowFullscreen(uint flags);
	uint windowFlags() const;
	bool isWindowFullscreen() const {
		return windowFlags() & (flag_fullscreen | flag_fullscreen_desktop);
	}

	double frameTime() const { return m_frame_time; }

	void grabMouse(bool);
	void showCursor(bool);

	const InputState &inputState() const { return m_input_state; }
	const vector<InputEvent> &inputEvents() const { return m_input_events; }

	using MainLoopFunction = bool (*)(GfxDevice &device);
	void runMainLoop(MainLoopFunction);

	static void clearColor(Color color);
	static void clearDepth(float depth_value);

	string extensions() const;

  private:
	int translateToSDL(int) const;
	int translateFromSDL(int) const;
	bool pollEvents();

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
	double m_last_time, m_frame_time;

	struct WindowImpl;
	unique_ptr<WindowImpl> m_window_impl;
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

class VertexBuffer : public immutable_base<VertexBuffer> {
  public:
	VertexBuffer(const void *data, int size, int vertex_size, VertexDataType);
	~VertexBuffer();

	void operator=(const VertexBuffer &) = delete;
	VertexBuffer(const VertexBuffer &) = delete;

	template <class T> static immutable_ptr<VertexBuffer> make(const vector<T> &data) {
		return make_immutable<VertexBuffer>(data);
	}

	template <class T>
	VertexBuffer(CRange<T> data)
		: VertexBuffer(data.data(), data.size(), (int)sizeof(T), TVertexDataType<T>()) {}
	template <class Range, typename = typename std::enable_if<ConvertsToRange<Range>::value>::type>
	VertexBuffer(const Range &range)
		: VertexBuffer(makeRange(range)) {}

	template <class T> vector<T> getData() const {
		ASSERT(TVertexDataType<T>().type == m_data_type.type);
		ASSERT(sizeof(T) == m_vertex_size);
		vector<T> out(m_size);
		download(reinterpretRange<char>(Range<T>(out)));
		return out;
	}

	int size() const { return m_size; }

  private:
	void download(Range<char> out, int src_offset = 0) const;

	unsigned m_handle;
	int m_size, m_vertex_size;
	VertexDataType m_data_type;
	friend class VertexArray;
};

using PVertexBuffer = immutable_ptr<VertexBuffer>;

class IndexBuffer : public immutable_base<IndexBuffer> {
  public:
	enum { max_index_value = 65535 };

	IndexBuffer(const vector<uint> &indices);
	~IndexBuffer();

	void operator=(const IndexBuffer &) = delete;
	IndexBuffer(const IndexBuffer &) = delete;

	vector<uint> getData() const;
	int size() const { return m_size; }

  private:
	enum IndexType {
		type_uint,
		type_ubyte,
		type_ushort,
	};

	unsigned m_handle;
	int m_size, m_index_size;
	IndexType m_index_type;
	friend class VertexArray;
};

using PIndexBuffer = immutable_ptr<IndexBuffer>;

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
	PVertexBuffer buffer() const { return m_buffer; }
	const float4 &singleValue() const { return m_single_value; }
	int offset() const { return m_offset; }

  private:
	friend class VertexArray;
	PVertexBuffer m_buffer;
	float4 m_single_value;
	int m_offset;
};

class VertexArray : public immutable_base<VertexArray> {
  public:
	using Source = VertexArraySource;

	VertexArray(vector<Source>, PIndexBuffer = PIndexBuffer());
	~VertexArray();

	static immutable_ptr<VertexArray> make(vector<Source> sources,
										   PIndexBuffer index_buffer = PIndexBuffer()) {
		return make_immutable<VertexArray>(std::move(sources), std::move(index_buffer));
	}

	void operator=(const VertexArray &) = delete;
	VertexArray(const VertexArray &) = delete;

	void draw(PrimitiveType::Type, int num_vertices, int offset = 0) const;
	void draw(PrimitiveType::Type primitive_type) const { draw(primitive_type, size()); }

	const auto &sources() const { return m_sources; }
	PIndexBuffer indexBuffer() const { return m_index_buffer; }
	int size() const { return m_size; }

  private:
	void init();
	void bind() const;
	bool bindVertexBuffer(int n) const;
	static void unbind();

	vector<Source> m_sources;
	PIndexBuffer m_index_buffer;
	int m_size;
#if OPENGL_VERSION >= 0x30
	unsigned m_handle;
#else
	static int s_max_bind;
#endif
};

using PVertexArray = immutable_ptr<VertexArray>;

class DrawCall {
  public:
	DrawCall(PVertexArray, PrimitiveType::Type, int vertex_count, int index_offset);
	void issue() const;

  private:
	PVertexArray m_vertex_array;
	PrimitiveType::Type m_primitive_type;
	int m_vertex_count, m_index_offset;
};

DECLARE_ENUM(ShaderType, vertex, fragment);

class Shader {
  public:
	using Type = ShaderType::Type;

	Shader(Type, Stream &, const string &predefined_macros = string());
	Shader(Type, const string &source, const string &predefined_macros = string(),
		   const string &name = string());
	Shader(Shader &&);
	~Shader();

	Shader(const Shader &) = delete;
	void operator=(const Shader &) = delete;

	Type type() const;
	string source() const;
	uint id() const { return m_id; }
	bool isValid() const { return m_id != 0; }

  private:
	uint m_id;
};

class Program : public immutable_base<Program> {
  public:
	Program(const Shader &vertex, const Shader &fragment,
			const vector<string> &location_names = {});
	Program(const string &vsh_file_name, const string &fsh_file_name,
			const string &predefined_macros, const vector<string> &location_names = {});
	~Program();

	Program(const Program &) = delete;
	void operator=(const Program &) = delete;

	unsigned id() const { return m_id; }
	string getInfo() const;

  private:
	uint m_id;
};

using PProgram = immutable_ptr<Program>;

class ProgramBinder {
  public:
	ProgramBinder(PProgram);
	ProgramBinder(const ProgramBinder &) = delete;
	ProgramBinder(ProgramBinder &&) = default;
	~ProgramBinder();

	void setUniform(const char *name, float);
	void setUniform(const char *name, CRange<float>);
	void setUniform(const char *name, CRange<float2>);
	void setUniform(const char *name, CRange<float3>);
	void setUniform(const char *name, CRange<float4>);
	void setUniform(const char *name, CRange<Matrix4>);

	void setUniform(const char *name, int);
	void setUniform(const char *name, const int2 &);
	void setUniform(const char *name, const int3 &);
	void setUniform(const char *name, const int4 &);
	void setUniform(const char *name, const float2 &);
	void setUniform(const char *name, const float3 &);
	void setUniform(const char *name, const float4 &);
	void setUniform(const char *name, const Matrix4 &);
	int getUniformLocation(const char *name) const;

	PProgram program() const { return m_program; }

	void bind() const;
	static void unbind();

  protected:
	PProgram m_program;
	unsigned id() const { return m_program->id(); }
};

class Material : public immutable_base<Material> {
  public:
	enum Flags {
		flag_blended = 1u,
		flag_two_sided = 2u,
		flag_clear_depth = 8u,
		flag_ignore_depth = 16u,
	};
	Material(vector<PTexture> textures, Color color = Color::white, uint flags = 0);
	Material(PTexture texture, Color color = Color::white, uint flags = 0);
	Material(Color color = Color::white, uint flags = 0);

	PTexture texture() const { return m_textures.empty() ? PTexture() : m_textures.front(); }
	const vector<PTexture> &textures() const { return m_textures; }
	Color color() const { return m_color; }
	uint flags() const { return m_flags; }

	bool operator<(const Material &rhs) const;

  protected:
	vector<PTexture> m_textures;
	Color m_color;
	uint m_flags;
};

using PMaterial = immutable_ptr<Material>;

class MaterialSet : public immutable_base<MaterialSet> {
  public:
	MaterialSet(PMaterial default_mat, std::map<string, PMaterial> = {});
	~MaterialSet();

	PMaterial operator[](const string &name) const;
	vector<PMaterial> operator()(const vector<string> &names) const;
	const auto &map() const { return m_map; }

  private:
	PMaterial m_default;
	std::map<string, PMaterial> m_map;
};

using PMaterialSet = immutable_ptr<MaterialSet>;

class Renderer;

struct Pose : public immutable_base<Pose> {
  public:
	using NameMap = vector<pair<string, int>>;

	Pose(vector<Matrix4> transforms = {}, NameMap = {});
	Pose(vector<Matrix4> transforms, const vector<string> &names);

	auto size() const { return m_transforms.size(); }
	const auto &transforms() const { return m_transforms; }
	const NameMap &nameMap() const { return m_name_map; }

	vector<int> mapNames(const vector<string> &) const;
	vector<Matrix4> mapTransforms(const vector<int> &mapping) const;

  private:
	NameMap m_name_map;
	vector<Matrix4> m_transforms;
};

using PPose = immutable_ptr<Pose>;

struct MeshBuffers {
  public:
	struct VertexWeight {
		VertexWeight(float weight = 0.0f, int node_id = 0) : weight(weight), node_id(node_id) {}

		float weight;
		int node_id;
	};

	MeshBuffers() = default;
	MeshBuffers(vector<float3> positions, vector<float3> normals = {},
				vector<float2> tex_coords = {}, vector<vector<VertexWeight>> weights = {},
				vector<string> node_names = {});
	MeshBuffers(PVertexBuffer positions, PVertexBuffer normals = PVertexBuffer(),
				PVertexBuffer tex_coords = PVertexBuffer());
	MeshBuffers(PVertexArray, int positions_id, int normals_id = -1, int tex_coords_id = -1);

	MeshBuffers(const XMLNode &node);
	void saveToXML(XMLNode) const;

	vector<float3> animatePositions(CRange<Matrix4>) const;
	vector<float3> animateNormals(CRange<Matrix4>) const;

	int size() const { return (int)positions.size(); }
	bool hasSkin() const { return !weights.empty() && !node_names.empty(); }

	static MeshBuffers transform(const Matrix4 &, MeshBuffers);
	MeshBuffers remap(const vector<uint> &mapping) const;

	// Pose must have configuration for each of the nodes in node_names
	vector<Matrix4> mapPose(PPose skinning_pose) const;

	vector<float3> positions;
	vector<float3> normals;
	vector<float2> tex_coords;
	vector<vector<VertexWeight>> weights;
	vector<string> node_names;
};

class MeshIndices {
  public:
	using Type = PrimitiveType::Type;
	using TriIndices = array<uint, 3>;

	static bool isSupported(Type type) {
		return isOneOf(type, Type::triangles, Type::triangle_strip);
	}

	MeshIndices(vector<uint> = {}, Type ptype = Type::triangles);
	MeshIndices(PIndexBuffer, Type ptype = Type::triangles);
	MeshIndices(const vector<TriIndices> &);

	static MeshIndices makeRange(int count, uint first = 0, Type ptype = Type::triangles);

	operator const vector<uint> &() const { return m_data; }
	Type type() const { return m_type; }

	vector<TriIndices> trisIndices() const;

	// Does not exclude degenerate triangles
	int triangleCount() const;
	int size() const { return (int)m_data.size(); }
	bool empty() const { return m_data.empty(); }
	pair<uint, uint> indexRange() const;

	static MeshIndices changeType(MeshIndices, Type new_type);
	static MeshIndices merge(const vector<MeshIndices> &, vector<pair<uint, uint>> &index_ranges);
	static MeshIndices applyOffset(MeshIndices, uint offset);

	vector<MeshIndices> split(uint max_vertices, vector<vector<uint>> &out_mappings) const;

  private:
	vector<uint> m_data;
	Type m_type;
};

// Make it immutable, with operations requiring a copy/move
class Mesh : public immutable_base<Mesh> {
  public:
	Mesh(MeshBuffers buffers = MeshBuffers(), vector<MeshIndices> indices = {},
		 vector<string> mat_names = {});
	Mesh(const Mesh &) = default;
	Mesh(Mesh &&) = default;
	Mesh &operator=(Mesh &&) = default;
	Mesh &operator=(const Mesh &) = default;

	Mesh(const XMLNode &);
	void saveToXML(XMLNode) const;

	static Mesh makeRect(const FRect &xz_rect, float y);
	static Mesh makeBBox(const FBox &bbox);
	static Mesh makeCylinder(const Cylinder &, int num_sides);
	static Mesh makeTetrahedron(const Tetrahedron &);

	struct AnimatedData {
		FBox bounding_box;
		vector<float3> positions;
		vector<float3> normals;
	};

	FBox boundingBox() const;
	FBox boundingBox(const AnimatedData &) const;

	void transformUV(const Matrix4 &);
	void changePrimitiveType(PrimitiveType::Type new_type);

	int vertexCount() const { return (int)m_buffers.positions.size(); }
	int triangleCount() const;

	const MeshBuffers &buffers() const { return m_buffers; }
	const vector<float3> &positions() const { return m_buffers.positions; }
	const vector<float3> &normals() const { return m_buffers.normals; }
	const vector<float2> &texCoords() const { return m_buffers.tex_coords; }
	const vector<MeshIndices> &indices() const { return m_indices; }
	const MeshIndices &mergedIndices() const { return m_merged_indices; }
	const auto &materialNames() const { return m_material_names; }

	bool hasTexCoords() const { return !m_buffers.tex_coords.empty(); }
	bool hasNormals() const { return !m_buffers.normals.empty(); }
	bool hasSkin() const { return m_buffers.hasSkin(); }
	bool isEmpty() const { return m_buffers.positions.empty(); }

	// TODO: make this completely immutable (and the others as well, if possible)
	void removeNormals() { m_buffers.normals.clear(); }
	void removeTexCoords() { m_buffers.tex_coords.clear(); }
	void process(float unit);

	using TriIndices = MeshIndices::TriIndices;
	vector<Triangle> tris() const;
	vector<TriIndices> trisIndices() const;

	vector<Mesh> split(int max_vertices) const;
	vector<Mesh> splitToSubMeshes() const;

	static Mesh merge(vector<Mesh>);
	static Mesh csgDifference(const Mesh &, const Mesh &);
	static Mesh csgUnion(const Mesh &, const Mesh &);
	static Mesh csgIntersection(const Mesh &, const Mesh &);

	static Mesh transform(const Matrix4 &, Mesh);

	float intersect(const Segment &) const;
	float intersect(const Segment &, const AnimatedData &) const;

	AnimatedData animate(PPose) const;
	Mesh animate(AnimatedData) const;
	void draw(Renderer &, const MaterialSet &, const Matrix4 &matrix = Matrix4::identity()) const;
	void draw(Renderer &, AnimatedData, const MaterialSet &,
			  const Matrix4 &matrix = Matrix4::identity()) const;
	void clearDrawingCache() const;

	bool isValidAnimationData(const AnimatedData &data) const {
		if(!hasSkin())
			return data.positions.empty() && data.normals.empty();
		return data.positions.size() == positions().size() &&
			   data.normals.size() == normals().size();
	}

	/*
		void genAdjacency();

		int faceCount() const;
		void getFace(int face, int &i1, int &i2, int &i3) const;
	*/

  protected:
	void verifyData() const;
	void updateDrawingCache() const;

	MeshBuffers m_buffers;
	vector<MeshIndices> m_indices;
	vector<string> m_material_names;

	MeshIndices m_merged_indices;
	vector<pair<uint, uint>> m_merged_ranges;

	mutable FBox m_bounding_box;
	mutable vector<pair<DrawCall, string>> m_drawing_cache;

	enum Flags {
		flag_bounding_box,
		flag_drawing_cache,
	};
	mutable uint m_ready_flags;
};

using PMesh = immutable_ptr<Mesh>;

class HalfMesh {
  public:
	using TriIndices = Mesh::TriIndices;
	class Vertex;
	class HalfEdge;
	class Face;

	HalfMesh(CRange<float3> positions = CRange<float3>(),
			 CRange<TriIndices> tri_indices = CRange<TriIndices>());
	HalfMesh(const Mesh &mesh) : HalfMesh(mesh.positions(), mesh.trisIndices()) {}

	bool is2ManifoldUnion() const;
	bool is2Manifold() const;

	bool empty() const { return m_faces.empty(); }

	Vertex *addVertex(const float3 &pos);
	Face *addFace(Vertex *a, Vertex *b, Vertex *c);
	vector<HalfEdge *> orderedEdges(Vertex *vert);
	void removeVertex(Vertex *vert);
	void removeFace(Face *face);
	vector<Vertex *> verts();
	vector<Face *> faces();
	vector<HalfEdge *> halfEdges();

	Face *findFace(Vertex *a, Vertex *b, Vertex *c);

	void clearTemps(int value);
	void selectConnected(Vertex *vert);
	HalfMesh extractSelection();

	void draw(Renderer &out, float scale);

	class Vertex {
	  public:
		Vertex(float3 pos, int index);
		~Vertex();
		Vertex(const Vertex &) = delete;
		void operator=(const Vertex &) = delete;

		const float3 &pos() const { return m_pos; }
		HalfEdge *first() { return m_edges.empty() ? nullptr : m_edges.front(); }
		const vector<HalfEdge *> &all() { return m_edges; }
		int degree() const { return (int)m_edges.size(); }

		int temp() const { return m_temp; }
		void setTemp(int temp) { m_temp = temp; }

		// It might change when verts are removed from HalfMesh
		int index() const { return m_index; }

	  private:
		void removeEdge(HalfEdge *edge);
		void addEdge(HalfEdge *edge);

		friend class HalfMesh;
		friend class HalfEdge;
		friend class Face;

		vector<HalfEdge *> m_edges;
		float3 m_pos;
		int m_index, m_temp;
	};

	class HalfEdge {
	  public:
		~HalfEdge();
		HalfEdge(const HalfEdge &) = delete;
		void operator=(const HalfEdge &) = delete;

		Vertex *start() { return m_start; }
		Vertex *end() { return m_end; }

		HalfEdge *opposite() { return m_opposite; }
		HalfEdge *next() { return m_next; }
		HalfEdge *prev() { return m_prev; }

		HalfEdge *prevVert() { return m_prev->m_opposite; }
		HalfEdge *nextVert() { return m_opposite ? m_opposite->m_next : nullptr; }
		Face *face() { return m_face; }

	  private:
		HalfEdge(Vertex *v1, Vertex *v2, HalfEdge *next, HalfEdge *prev, Face *face);

		friend class Face;
		Vertex *m_start, *m_end;
		HalfEdge *m_opposite, *m_next, *m_prev;
		Face *m_face;
	};

	class Face {
	  public:
		Face(Vertex *v1, Vertex *v2, Vertex *v3, int index);

		HalfEdge *halfEdge(int idx) {
			DASSERT(idx >= 0 && idx <= 2);
			return idx == 0 ? &m_he0 : idx == 1 ? &m_he1 : &m_he2;
		}

		array<Vertex *, 3> verts() { return {{m_he0.start(), m_he1.start(), m_he2.start()}}; }
		array<HalfEdge *, 3> halfEdges() { return {{&m_he0, &m_he1, &m_he2}}; }

		const auto &triangle() const { return m_tri; }
		void setTemp(int temp) { m_temp = temp; }
		int temp() const { return m_temp; }

		// It might change when faces are removed from HalfMesh
		int index() const { return m_index; }

	  private:
		friend class HalfMesh;
		HalfEdge m_he0, m_he1, m_he2;
		Triangle m_tri;
		int m_index, m_temp;
	};

	vector<unique_ptr<Vertex>> m_verts;
	vector<unique_ptr<Face>> m_faces;
};

class TetMesh : public immutable_base<TetMesh> {
  public:
	using TriIndices = MeshIndices::TriIndices;
	using TetIndices = array<int, 4>;

	TetMesh(vector<float3> positions = vector<float3>(),
			CRange<TetIndices> tet_verts = CRange<TetIndices>());

	array<int, 3> tetFace(int tet, int face_id) const {
		DASSERT(tet >= 0 && tet < (int)m_tet_tets.size());
		DASSERT(face_id >= 0 && face_id < 4);
		static int inds[4][3] = {{0, 2, 1}, {1, 2, 3}, {2, 0, 3}, {3, 0, 1}};
		const auto &tverts = m_tet_verts[tet];
		return {{tverts[inds[face_id][0]], tverts[inds[face_id][1]], tverts[inds[face_id][2]]}};
	}
	Tetrahedron makeTet(int tet) const;

	enum Flags {
		flag_quality = 1,
		flag_print_details = 2,
	};

	static TetMesh transform(const Matrix4 &, TetMesh);
	static Mesh findIntersections(const Mesh &);
	static TetMesh make(const Mesh &, uint flags = 0);
	static TetMesh makeUnion(const vector<TetMesh> &);
	static TetMesh selectTets(const TetMesh &, const vector<int> &indices);
	static TetMesh boundaryIsect(const TetMesh &, const TetMesh &, vector<Segment> &);

	void drawLines(Renderer &, PMaterial, const Matrix4 &matrix = Matrix4::identity()) const;
	void drawTets(Renderer &, PMaterial, const Matrix4 &matrix = Matrix4::identity()) const;

	const auto &verts() const { return m_verts; }
	const auto &tetVerts() const { return m_tet_verts; }
	const auto &tetTets() const { return m_tet_tets; }
	int size() const { return (int)m_tet_tets.size(); }

	Mesh toMesh() const;

  private:
	vector<float3> m_verts;
	vector<array<int, 4>> m_tet_tets;
	vector<array<int, 4>> m_tet_verts;
};

using PTetMesh = immutable_ptr<TetMesh>;

struct MaterialDef {
	MaterialDef(const string &name, Color diffuse) : name(name), diffuse(diffuse) {}
	MaterialDef(const XMLNode &);
	void saveToXML(XMLNode) const;

	string name;
	Color diffuse;
};

class Model;

class ModelAnim {
  public:
	ModelAnim(const XMLNode &, PPose default_pose);
	void saveToXML(XMLNode) const;

	string print() const;
	const string &name() const { return m_name; }
	float length() const { return m_length; }
	// TODO: advanced interpolation support

	static void transToXML(const AffineTrans &trans, const AffineTrans &default_trans,
						   XMLNode node);
	static AffineTrans transFromXML(XMLNode node, const AffineTrans &default_trans = AffineTrans());

	PPose animatePose(PPose initial_pose, double anim_time) const;

  protected:
	string m_name;

	AffineTrans animateChannel(int channel_id, double anim_pos) const;
	void verifyData() const;

	struct Channel {
		Channel() = default;
		Channel(const XMLNode &, const AffineTrans &default_trans);
		void saveToXML(XMLNode) const;
		AffineTrans blend(int frame0, int frame1, float t) const;

		// TODO: interpolation information
		AffineTrans trans, default_trans;
		vector<float3> translation_track;
		vector<float3> scaling_track;
		vector<Quat> rotation_track;
		vector<float> time_track;
		string node_name;
	};

	// TODO: information about whether its looped or not
	vector<Channel> m_channels;
	vector<float> m_shared_time_track;
	vector<string> m_node_names;
	float m_length;
};

class ModelNode;
using PModelNode = unique_ptr<ModelNode>;

DECLARE_ENUM(ModelNodeType, generic, mesh, armature, bone, empty);

class ModelNode {
  public:
	using Type = ModelNodeType::Type;

	struct Property {
		bool operator<(const Property &rhs) const {
			return tie(name, value) < tie(rhs.name, rhs.value);
		}

		string name;
		string value;
	};

	using PropertyMap = std::map<string, string>;

	ModelNode(const string &name, Type type = Type::generic,
			  const AffineTrans &trans = AffineTrans(), PMesh mesh = PMesh(),
			  vector<Property> props = {});
	ModelNode(const ModelNode &rhs);

	void addChild(PModelNode);
	PModelNode removeChild(const ModelNode *);
	PModelNode clone() const;

	Type type() const { return m_type; }
	const auto &children() const { return m_children; }
	const auto &name() const { return m_name; }
	const auto &properties() const { return m_properties; }
	PropertyMap propertyMap() const;
	const ModelNode *find(const string &name, bool recursive = true) const;

	// TODO: name validation
	void setTrans(const AffineTrans &trans);
	void setName(const string &name) { m_name = name; }
	void setMesh(PMesh mesh) { m_mesh = std::move(mesh); }
	void setId(int new_id) { m_id = new_id; }

	const auto &localTrans() const { return m_trans; }
	const auto &invLocalTrans() const { return m_inv_trans; }

	Matrix4 globalTrans() const;
	Matrix4 invGlobalTrans() const;

	auto mesh() const { return m_mesh; }
	int id() const { return m_id; }

	const ModelNode *root() const;
	const ModelNode *parent() const { return m_parent; }
	bool isDescendant(const ModelNode *ancestor) const;

	void dfs(vector<ModelNode *> &out);
	bool join(const ModelNode *other, const string &name);

  private:
	vector<PModelNode> m_children;
	vector<Property> m_properties;
	string m_name;
	AffineTrans m_trans;
	Matrix4 m_inv_trans;
	PMesh m_mesh;
	Type m_type;
	int m_id;
	const ModelNode *m_parent;
};

class Model : public immutable_base<Model> {
  public:
	Model() = default;
	Model(Model &&) = default;
	Model(const Model &);
	Model &operator=(Model &&) = default;
	~Model() = default;

	Model(PModelNode, vector<ModelAnim> anims = {}, vector<MaterialDef> material_defs = {});
	static Model loadFromXML(const XMLNode &);
	void saveToXML(XMLNode) const;

	// TODO: use this in transformation functions
	// TODO: better name
	/*	void decimate(MeshBuffers &out_buffers, vector<MeshIndices> &out_indices,
					  vector<string> &out_names) {
			out_buffers = std::move(m_buffers);
			out_indices = std::move(m_indices);
			out_names = std::move(m_material_names);
			*this = Mesh();
		}*/

	void join(const string &local_name, const Model &, const string &other_name);

	const ModelNode *findNode(const string &name) const { return m_root->find(name); }
	int findNodeId(const string &name) const;
	const ModelNode *rootNode() const { return m_root.get(); }
	const auto &nodes() const { return m_nodes; }

	const auto &anims() const { return m_anims; }

	const auto &materialDefs() const { return m_material_defs; }

	Mesh toMesh(PPose = PPose()) const;
	void printHierarchy() const;

	FBox boundingBox(PPose = PPose()) const;
	Matrix4 nodeTrans(const string &name, PPose = PPose()) const;

	float intersect(const Segment &, PPose = PPose()) const;

	void draw(Renderer &out, const MaterialSet &mats,
			  const Matrix4 &matrix = Matrix4::identity()) const {
		draw(out, defaultPose(), mats, matrix);
	}
	void draw(Renderer &, PPose, const MaterialSet &,
			  const Matrix4 &matrix = Matrix4::identity()) const;
	void drawNodes(Renderer &, PPose, Color node_color, Color line_color, float node_scale = 1.0f,
				   const Matrix4 &matrix = Matrix4::identity()) const;

	void clearDrawingCache() const;

	const ModelAnim &anim(int anim_id) const { return m_anims[anim_id]; }
	int animCount() const { return (int)m_anims.size(); }

	// Pass -1 to anim_id for bind position
	PPose animatePose(int anim_id, double anim_pos) const;
	PPose defaultPose() const { return m_default_pose; }
	PPose globalPose(PPose local_pose) const;
	PPose meshSkinningPose(PPose global_pose, int node_id) const;
	bool isValidPose(PPose) const;
	void process(float unit);

  protected:
	void updateNodes();
	void verifyData() const;

	struct AnimatedData : public immutable_base<AnimatedData> {
		PPose global_pose;
		struct MeshData {
			PMesh mesh;
			Mesh::AnimatedData anim_data;
			Matrix4 transform;
		};
		vector<MeshData> meshes_data;
	};
	immutable_ptr<AnimatedData> animatedData(PPose) const;

	PModelNode m_root;
	vector<ModelAnim> m_anims;

	vector<MaterialDef> m_material_defs;

	vector<ModelNode *> m_nodes;
	PPose m_default_pose;
};

using PModel = immutable_ptr<Model>;

template <class T> class XMLLoader : public ResourceLoader<T> {
  public:
	XMLLoader(const string &prefix, const string &suffix, string node_name)
		: ResourceLoader<T>(prefix, suffix), m_node_name(std::move(node_name)) {}

	immutable_ptr<T> operator()(const string &name) {
		XMLDocument doc;
		Loader(ResourceLoader<T>::fileName(name)) >> doc;
		XMLNode child = doc.child(m_node_name.empty() ? nullptr : m_node_name.c_str());
		if(!child)
			THROW("Cannot find node '%s' in XML document", m_node_name.c_str());
		return immutable_ptr<T>(T::loadFromXML(child));
	}

  protected:
	string m_node_name;
};

class RenderBuffer {
  public:
	using Format = TextureFormat;
	RenderBuffer(TextureFormat, const int2 &size);
	~RenderBuffer();

	void operator=(const RenderBuffer &) = delete;
	RenderBuffer(const RenderBuffer &) = delete;

	Format format() const { return m_format; }
	uint id() const { return m_id; }
	int2 size() const { return m_size; }

  private:
	int2 m_size;
	Format m_format;
	uint m_id;
};

using SRenderBuffer = shared_ptr<RenderBuffer>;

struct FrameBufferTarget {
	FrameBufferTarget() {}
	FrameBufferTarget(STexture texture) : texture(std::move(texture)) {}
	FrameBufferTarget(SRenderBuffer render_buffer) : render_buffer(std::move(render_buffer)) {}

	operator bool() const;
	TextureFormat format() const;
	int2 size() const;

	STexture texture;
	SRenderBuffer render_buffer;
};

class FrameBuffer {
  public:
	using Target = FrameBufferTarget;
	FrameBuffer(vector<Target> colors, Target depth = Target());
	FrameBuffer(Target color, Target depth = Target());
	~FrameBuffer();

	static shared_ptr<FrameBuffer> make(vector<Target> colors, Target depth = Target()) {
		return make_shared<FrameBuffer>(std::move(colors), std::move(depth));
	}

	void operator=(const FrameBuffer &) = delete;
	FrameBuffer(const FrameBuffer &) = delete;

	void bind();
	static void unbind();

	const auto &colors() const { return m_colors; }
	const Target &depth() const { return m_depth; }
	int2 size() const;

  private:
	vector<Target> m_colors;
	Target m_depth;
	uint m_id;
};

using SFrameBuffer = shared_ptr<FrameBuffer>;

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
	mutable Frustum m_frustum;
	mutable bool m_is_dirty, m_is_frustum_dirty;
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
	Renderer(const IRect &viewport, const Matrix4 &projection_matrix = Matrix4::identity());
	virtual ~Renderer();

	virtual void render();
	void clear();

	void addDrawCall(const DrawCall &, PMaterial, const Matrix4 &matrix = Matrix4::identity());
	void addLines(CRange<float3>, PMaterial, const Matrix4 &matrix = Matrix4::identity());
	void addLines(CRange<float3>, Color, const Matrix4 &matrix = Matrix4::identity());
	void addSegments(CRange<Segment>, PMaterial, const Matrix4 &matrix = Matrix4::identity());

	void addWireBox(const FBox &bbox, Color color, const Matrix4 &matrix = Matrix4::identity());
	void addSprite(CRange<float3, 4> verts, CRange<float2, 4> tex_coords, PMaterial,
				   const Matrix4 &matrix = Matrix4::identity());

	// TODO: this is useful
	// Each line is represented by two vertices
	//	void addLines(const float3 *pos, const Color *color, int num_lines, const Material
	//&material);

	// TODO: pass ElementSource class, which can be single element, vector, pod array, whatever
  protected:
	void renderLines();
	void renderSprites();

	struct Instance {
		Matrix4 matrix;
		PMaterial material;
		DrawCall draw_call;
	};

	struct SpriteInstance {
		Matrix4 matrix;
		PMaterial material;
		array<float3, 4> verts;
		array<float2, 4> tex_coords;
	};

	struct LineInstance {
		Matrix4 matrix;
		int first, count;
		uint material_flags;
	};

	IRect m_viewport;
	vector<SpriteInstance> m_sprites;

	vector<LineInstance> m_lines;
	vector<float3> m_line_positions;
	vector<Color> m_line_colors;
	vector<uint> m_line_flags;

	vector<Instance> m_instances;
	PProgram m_tex_program, m_flat_program, m_simple_program;
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

class Font : public immutable_base<Font> {
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

using PFont = immutable_ptr<Font>;

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

	IRect evalExtents(const TextFormatter &fmt, bool exact = false) const {
		return m_font->evalExtents(fmt.text(), exact);
	}
	IRect evalExtents(const string &str, bool exact = false) const {
		return m_font->evalExtents(str.c_str(), exact);
	}

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
