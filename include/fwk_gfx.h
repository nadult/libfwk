/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_GFX_H
#define FWK_GFX_H

#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

class InputState;
class InputEvent;

DEFINE_ENUM(ColorId, white, gray, yellow, cyan, magneta, purple, brown, orange, gold, red, green,
			blue, black, transparent);

struct FColor {
	FColor() : r(0.0), g(0.0), b(0.0), a(1.0) {}
	FColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
	FColor(const float4 &rgba) : r(rgba[0]), g(rgba[1]), b(rgba[2]), a(rgba[3]) {}
	FColor(const FColor &col, float a) : r(col.r), g(col.g), b(col.b), a(a) {}
	FColor(const float3 &rgb, float a = 1.0) : r(rgb[0]), g(rgb[1]), b(rgb[2]), a(a) {}
	FColor(ColorId);
	operator float4() const { return float4(v); }

	float3 rgb() const { return float3(r, g, b); }
	FColor operator*(float s) const { return FColor(r * s, g * s, b * s, a * s); }
	FColor operator*(const FColor &rhs) const {
		return FColor(r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a);
	}
	FColor operator-(const FColor &rhs) const {
		return FColor(r - rhs.r, g - rhs.g, b - rhs.b, a - rhs.a);
	}
	FColor operator+(const FColor &rhs) const {
		return FColor(r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a);
	}

	auto tied() const { return std::tie(r, g, b, a); }
	bool operator<(const FColor &rhs) const { return tied() < rhs.tied(); }
	bool operator==(const FColor &rhs) const { return tied() == rhs.tied(); }

	union {
		struct {
			float r, g, b, a;
		};
		float v[4];
	};
};

FColor mulAlpha(FColor color, float alpha);
FColor desaturate(FColor col, float value);
FColor srgbToLinear(const FColor &);
FColor linearToSrgb(const FColor &);
float3 hsvToRgb(float3);
float3 rgbToHsv(float3);
inline float3 rgbToHsv(const FColor &col) { return rgbToHsv(col.rgb()); }

struct IColor {
	IColor(u8 r, u8 g, u8 b, u8 a = 255) : r(r), g(g), b(b), a(a) {}
	IColor(int4 rgba) : IColor(rgba[0], rgba[1], rgba[2], rgba[3]) {}
	IColor(int r, int g, int b, int a = 255)
		: r(clamp(r, 0, 255)), g(clamp(g, 0, 255)), b(clamp(b, 0, 255)), a(clamp(a, 0, 255)) {}
	explicit IColor(const FColor &c)
		: r(clamp(c.r * 255.0f, 0.0f, 255.0f)), g(clamp(c.g * 255.0f, 0.0f, 255.0f)),
		  b(clamp(c.b * 255.0f, 0.0f, 255.0f)), a(clamp(c.a * 255.0f, 0.0f, 255.0f)) {}
	IColor(IColor col, u8 alpha) : r(col.r), g(col.g), b(col.b), a(alpha) {}
	IColor(ColorId color_id) : IColor(FColor(color_id)) {}
	IColor() : IColor(0, 0, 0) {}

	operator FColor() const { return float4(r, g, b, a) * (1.0f / 255.0f); }
	operator int4() const { return int4(r, g, b, a); }

	auto tied() const { return std::tie(r, g, b, a); }
	bool operator<(const IColor &rhs) const { return tied() < rhs.tied(); }
	bool operator==(const IColor &rhs) const { return tied() == rhs.tied(); }

	IColor bgra() const { return IColor(b, g, r, a); }

	union {
		struct {
			u8 r, g, b, a;
		};
		u8 rgba[4];
	};
};

DEFINE_ENUM(TextureFormatId, rgba, rgba_f16, rgba_f32, rgb, rgb_f16, rgb_f32, luminance, dxt1, dxt3,
			dxt5, depth, depth_stencil);

class TextureFormat {
  public:
	using Id = TextureFormatId;

	TextureFormat(int internal, int format, int type);
	TextureFormat(Id id = Id::rgba) : m_id(id) {}

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
	// TODO: make it initialize with black or something by default
	Texture(int2);
	Texture(Stream &);
	Texture();

	// data is not preserved
	// TODO: it should be or else remove this function
	void resize(int2);
	void clear();
	void fill(IColor);
	void blit(const Texture &src, int2 target_pos);

	int width() const { return m_size.x; }
	int height() const { return m_size.y; }
	int2 size() const { return m_size; }
	int pixelCount() const { return m_size.x * m_size.y; }

	bool empty() const { return m_data.empty(); }
	bool testPixelAlpha(const int2 &) const;

	TextureFormat format() const { return TextureFormatId::rgba; }

	// Loading from TGA, BMP, PNG, DDS
	void load(Stream &);
	void save(Stream &) const;
	void swap(Texture &);

	using Loader = void (*)(Stream &, PodArray<IColor> &out_data, int2 &out_size);
	struct RegisterLoader {
		RegisterLoader(const char *locase_ext, Loader);
	};

	void saveTGA(Stream &) const;

	IColor *data() { return m_data.data(); }
	const IColor *data() const { return m_data.data(); }

	IColor *line(int y) {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}
	const IColor *line(int y) const {
		DASSERT(y < m_size.y);
		return &m_data[y * m_size.x];
	}

	IColor &operator()(int x, int y) { return m_data[x + y * m_size.x]; }
	const IColor operator()(int x, int y) const { return m_data[x + y * m_size.x]; }

	IColor &operator[](int idx) { return m_data[idx]; }
	const IColor operator[](int idx) const { return m_data[idx]; }

  private:
	PodArray<IColor> m_data;
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
		flag_maximized = 64u,
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

	const InputState &inputState() const;
	const vector<InputEvent> &inputEvents() const;

	using MainLoopFunction = bool (*)(GfxDevice &device);
	void runMainLoop(MainLoopFunction);

	static void clearColor(FColor);
	static void clearDepth(float depth_value);

	string extensions() const;

  private:
	bool pollEvents();

#ifdef __EMSCRIPTEN__
	static void emscriptenCallback();
#endif
	MainLoopFunction m_main_loop_function;

	struct InputImpl;
	unique_ptr<InputImpl> m_input_impl;
	double m_last_time, m_frame_time;

	struct WindowImpl;
	unique_ptr<WindowImpl> m_window_impl;
};

struct RectStyle {
	RectStyle(FColor fill_color = ColorId::white, FColor border_color = ColorId::transparent)
		: fill_color(fill_color), border_color(border_color) {}

	FColor fill_color;
	FColor border_color;
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
DECLARE_VERTEX_DATA(IColor, ubyte, 4, true)

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
	template <class Range, typename = typename std::enable_if<IsRange<Range>::value>::type>
	VertexBuffer(const Range &range) : VertexBuffer(makeRange(range)) {}

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

DEFINE_ENUM(PrimitiveType, points, lines, triangles, triangle_strip);

class VertexArraySource {
  public:
	VertexArraySource(PVertexBuffer buffer, int offset = 0);
	VertexArraySource(const float4 &value);
	VertexArraySource(const float3 &value) : VertexArraySource(float4(value, 0.0f)) {}
	VertexArraySource(const float2 &value) : VertexArraySource(float4(value, 0.0f, 0.0f)) {}
	VertexArraySource(float value) : VertexArraySource(float4(value, 0.0f, 0.0f, 0.0f)) {}
	VertexArraySource(IColor color) : VertexArraySource(FColor(color)) {}
	VertexArraySource(FColor color) : VertexArraySource(float4(color)) {}
	VertexArraySource(VertexArraySource &&) = default;
	VertexArraySource(const VertexArraySource &) = default;

	int maxSize() const;
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
		return make_immutable<VertexArray>(move(sources), move(index_buffer));
	}

	void operator=(const VertexArray &) = delete;
	VertexArray(const VertexArray &) = delete;

	void draw(PrimitiveType, int num_vertices, int offset = 0) const;
	void draw(PrimitiveType primitive_type) const { draw(primitive_type, size()); }

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
	FrameBufferTarget(STexture texture) : texture(move(texture)) {}
	FrameBufferTarget(SRenderBuffer render_buffer) : render_buffer(move(render_buffer)) {}

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
		return make_shared<FrameBuffer>(move(colors), move(depth));
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

DEFINE_ENUM(ShaderType, vertex, fragment);

class Shader {
  public:
	using Type = ShaderType;

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

class SimpleMaterial {
  public:
	SimpleMaterial(STexture texture, FColor color = ColorId::white)
		: m_texture(texture), m_color(color) {}
	SimpleMaterial(PTexture texture, FColor color = ColorId::white)
		: m_texture(texture), m_color(color) {}
	SimpleMaterial(FColor color = ColorId::white) : m_color(color) {}

	shared_ptr<const DTexture> texture() const { return m_texture; }
	FColor color() const { return m_color; }

  private:
	shared_ptr<const DTexture> m_texture;
	FColor m_color;
};

// TODO: no need to make it immutable, it should be able to store STexture as well
class Material : public immutable_base<Material> {
  public:
	enum Flags {
		flag_blended = 0x0001u,
		flag_two_sided = 0x0002u,
		flag_clear_depth = 0x0004u,
		flag_ignore_depth = 0x0008u,

		flag_custom_mask = 0xffff0000u,
		flag_custom_shift = 16,
	};

	Material(vector<PTexture> textures, FColor color = ColorId::white, uint flags = 0);
	Material(PTexture texture, FColor color = ColorId::white, uint flags = 0);
	Material(FColor color = ColorId::white, uint flags = 0);

	PTexture texture() const { return m_textures.empty() ? PTexture() : m_textures.front(); }
	const vector<PTexture> &textures() const { return m_textures; }
	FColor color() const { return m_color; }
	uint flags() const { return m_flags; }

	bool operator<(const Material &rhs) const;

  protected:
	vector<PTexture> m_textures;
	FColor m_color;
	uint m_flags;
};

using PMaterial = immutable_ptr<Material>;

class MaterialSet {
  public:
	MaterialSet(PMaterial default_mat, std::map<string, PMaterial> = {});
	~MaterialSet();

	auto defaultMat() const { return m_default; }
	PMaterial operator[](const string &name) const;
	vector<PMaterial> operator()(const vector<string> &names) const;
	const auto &map() const { return m_map; }

  private:
	PMaterial m_default;
	std::map<string, PMaterial> m_map;
};

class MatrixStack {
  public:
	MatrixStack(const Matrix4 &proj_matrix = Matrix4::identity(),
				const Matrix4 &view_matrix = Matrix4::identity());

	void pushViewMatrix();
	void popViewMatrix();
	void mulViewMatrix(const Matrix4 &);
	void setViewMatrix(const Matrix4 &);
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
	void setViewPos(const int2 &view_pos) { setViewPos(float2(view_pos)); }

	void render();

	void addFilledRect(const FRect &rect, const FRect &tex_rect, CRange<FColor, 4>,
					   const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const FRect &tex_rect, const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const SimpleMaterial &mat) {
		addFilledRect(rect, FRect({1, 1}), mat);
	}
	void addFilledRect(const IRect &rect, const SimpleMaterial &mat) {
		addFilledRect(FRect(rect), mat);
	}

	void addRect(const FRect &rect, FColor);
	void addRect(const IRect &rect, FColor color) { addRect(FRect(rect), color); }

	void addLine(const float2 &, const float2 &, FColor);
	void addLine(const int2 &p1, const int2 &p2, FColor color) {
		addLine(float2(p1), float2(p2), color);
	}

	struct Element {
		Matrix4 matrix;
		shared_ptr<const DTexture> texture;
		int first_index, num_indices;
		int scissor_rect_id;
		PrimitiveType primitive_type;
	};

	// tex_coord & color can be empty
	void addQuads(CRange<float2> pos, CRange<float2> tex_coord, CRange<FColor>,
				  const SimpleMaterial &);
	void addLines(CRange<float2> pos, CRange<FColor>, FColor mat_color);
	void addTris(CRange<float2> pos, CRange<float2> tex_coord, CRange<FColor>,
				 const SimpleMaterial &material);

	Maybe<IRect> scissorRect() const;
	void setScissorRect(Maybe<IRect>);

	void clear();
	const IRect &viewport() const { return m_viewport; }

  private:
	struct DrawChunk {
		void appendVertices(CRange<float2> pos, CRange<float2> tex_coord, CRange<FColor>, FColor);

		vector<float2> positions;
		vector<float2> tex_coords;
		vector<IColor> colors;
		vector<uint> indices;
		vector<Element> elements;
	};

	DrawChunk &allocChunk(int num_verts);
	Element &makeElement(DrawChunk &, PrimitiveType, shared_ptr<const DTexture>);

	vector<DrawChunk> m_chunks;
	vector<IRect> m_scissor_rects;
	IRect m_viewport;
	PProgram m_tex_program, m_flat_program;
	int m_current_scissor_rect;
};

class SpriteBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		PMaterial material;
		vector<float3> positions;
		vector<float2> tex_coords;
		vector<IColor> colors;
	};

	SpriteBuffer(const MatrixStack &);
	void add(CRange<float3> verts, CRange<float2> tex_coords, CRange<IColor> colors, PMaterial,
			 const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }

  private:
	Instance &instance(PMaterial, Matrix4, bool has_colors, bool has_tex_coords);

	vector<Instance> m_instances;
	const MatrixStack &m_matrix_stack;
};

class LineBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		vector<float3> positions;
		vector<IColor> colors;
		uint material_flags;
		FColor material_color;
	};

	LineBuffer(const MatrixStack &);
	void add(CRange<float3>, CRange<IColor>, PMaterial,
			 const Matrix4 &matrix = Matrix4::identity());
	void add(CRange<float3>, PMaterial, const Matrix4 &matrix = Matrix4::identity());
	void add(CRange<float3>, IColor, const Matrix4 &matrix = Matrix4::identity());
	void add(CRange<Segment3<float>>, PMaterial, const Matrix4 &matrix = Matrix4::identity());
	void addBox(const FBox &bbox, IColor color, const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }

  private:
	Instance &instance(FColor, uint, Matrix4, bool has_colors);

	vector<Instance> m_instances;
	const MatrixStack &m_matrix_stack;
};

class DrawCall {
  public:
	DrawCall(PVertexArray, PrimitiveType, int vertex_count, int index_offset,
			 PMaterial = PMaterial(), Matrix4 = Matrix4::identity(), Maybe<FBox> = none);

	void issue() const;

	Matrix4 matrix;
	Maybe<FBox> bbox;
	PMaterial material;

  private:
	PVertexArray m_vertex_array;
	PrimitiveType m_primitive_type;
	int m_vertex_count, m_index_offset;
};

class RenderList : public MatrixStack {
  public:
	RenderList(const IRect &viewport, const Matrix4 &projection_matrix = Matrix4::identity());
	RenderList(const RenderList &) = delete;
	void operator=(const RenderList &) = delete;
	~RenderList();

	void render();
	void clear();

	void add(DrawCall);
	void add(DrawCall, const Matrix4 &);
	void add(CRange<DrawCall>);
	void add(CRange<DrawCall>, const Matrix4 &);

	const auto &drawCalls() const { return m_draw_calls; }
	const auto &sprites() const { return m_sprites; }
	const auto &lines() const { return m_lines; }
	auto &sprites() { return m_sprites; }
	auto &lines() { return m_lines; }

  protected:
	void renderLines();
	void renderSprites();

	vector<DrawCall> m_draw_calls;
	SpriteBuffer m_sprites;
	LineBuffer m_lines;
	IRect m_viewport;
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
	FontStyle(FColor color, FColor shadow_color, HAlign halign = HAlign::left,
			  VAlign valign = VAlign::top);
	FontStyle(FColor color, HAlign halign = HAlign::left, VAlign valign = VAlign::top);

	FColor text_color;
	FColor shadow_color;
	HAlign halign;
	VAlign valign;
};

class FontCore : public immutable_base<FontCore> {
  public:
	FontCore(const string &name, Stream &);
	FontCore(const XMLDocument &);
	FontCore(const XMLNode &);

	const string &textureName() const { return m_texture_name; }

	struct Glyph {
		int character;
		short2 tex_pos;
		short2 size;
		short2 offset;
		short x_advance;
	};

	IRect evalExtents(const wstring &) const;
	int lineHeight() const { return m_line_height; }

  private:
	FontCore();
	void computeRect();

	friend class FontFactory;
	friend class Font;

	// Returns number of quads generated
	// For every quad it generates: 4 vectors in each buffer
	int genQuads(const wstring &, Range<float2> out_pos, Range<float2> out_uv) const;

	// TODO: better representation? hash table maybe?
	std::map<int, Glyph> m_glyphs;
	std::map<pair<int, int>, int> m_kernings;
	string m_texture_name;
	int2 m_texture_size;

	string m_face_name;
	IRect m_max_rect;
	int m_line_height;
};

using PFontCore = immutable_ptr<FontCore>;

class Font {
  public:
	using Style = FontStyle;
	Font(PFontCore font, PTexture texture);

	FRect draw(Renderer2D &out, const FRect &rect, const Style &style, const wstring &text) const;
	FRect draw(Renderer2D &out, const float2 &pos, const Style &style, const wstring &text) const {
		return draw(out, FRect(pos, pos), style, text);
	}

	FRect draw(Renderer2D &out, const FRect &rect, const Style &style, StringRef text_utf8) const {
		return draw(out, rect, style, toWideString(text_utf8));
	}
	FRect draw(Renderer2D &out, const float2 &pos, const Style &style, StringRef text_utf8) const {
		return draw(out, FRect(pos, pos), style, toWideString(text_utf8));
	}

	auto core() const { return m_core; }
	auto texture() const { return m_texture; }

	IRect evalExtents(const wstring &text) const { return m_core->evalExtents(text); }
	IRect evalExtents(StringRef text) const { return m_core->evalExtents(toWideString(text)); }
	int lineHeight() const { return m_core->lineHeight(); }

  private:
	PFontCore m_core;
	PTexture m_texture;
};

class FontFactory {
  public:
	FontFactory();
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	Font makeFont(string const &path, int size_in_pixels, bool lcd_mode = false);

  private:
	class Impl;
	unique_ptr<Impl> m_impl;
};
}

SERIALIZE_AS_POD(FColor)
SERIALIZE_AS_POD(IColor)

#endif
