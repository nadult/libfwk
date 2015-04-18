/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_GFX_H
#define FWK_GFX_H

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_input.h"
#include "fwk_xml.h"

namespace fwk {

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

	u8 r, g, b, a;
};

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
class Texture : public RefCounter {
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

// Device texture, no support for mipmaps,
class DTexture : public Resource {
  public:
	DTexture();
	DTexture(DTexture &&);
	DTexture(const DTexture &) = delete;
	void operator=(const DTexture &) = delete;
	void operator=(DTexture &&);
	~DTexture();

	void load(Stream &);

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
};

typedef Ptr<DTexture> PTexture;

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
	double m_time_pressed[InputKey::count];
	double m_last_time;
	double m_press_delay;
	int m_clock;
};

struct RectStyle {
	RectStyle(Color fill_color = Color::white, Color border_color = Color::transparent)
		: fill_color(fill_color), border_color(border_color) {}

	Color fill_color;
	Color border_color;
};

// TODO: fix VertexArray & VertexBuffer classes
// They don't really add much value on top of OpenGL functions

class VertexArray {
  public:
	VertexArray();
	~VertexArray();
	VertexArray(const VertexArray &) = delete;
	const VertexArray &operator=(const VertexArray &) = delete;

	VertexArray(VertexArray &&);
	void operator=(VertexArray &&);

	void bind() const;
	static void unbind();

	void addAttrib(int size, int type, bool normalze, int stride, int offset);
	void clear();

#if OPENGL_VERSION >= 0x30
	int handle() const { return (int)m_handle; }
#endif

  protected:
	int m_bind_count;
#if OPENGL_VERSION >= 0x30
	unsigned m_handle;
#else
	enum { max_binds = 8 };

	struct AttribBind {
		int handle, offset;
		u16 type, stride;
		u8 index, size;
		bool normalize;
	};

	AttribBind m_binds[max_binds];
#endif
};

// TODO: usage parameter in setData?
// TODO: make this type safe (split to base class and templated class)
class VertexBuffer {
  public:
	VertexBuffer();
	~VertexBuffer();

	VertexBuffer(VertexBuffer &&rhs) : m_handle(rhs.m_handle) { rhs.m_handle = 0; }
	VertexBuffer(const VertexBuffer &) = delete;

	const VertexBuffer &operator=(const VertexBuffer &) = delete;
	const VertexBuffer &operator=(VertexBuffer &&);

	void bind() const;
	static void unbind();

	int size() const { return m_size; }
	void getData(int offset, int count, void *ptr) const;
	void setData(int count, const void *ptr, unsigned usage);

	template <class T> void getData(PodArray<T> &out) const {
		out.resize(m_size / sizeof(T));
		getData(0, m_size, out.data());
	}

	template <class T> void setData(const PodArray<T> &in, unsigned usage) {
		setData(in.size() * sizeof(T), in.data(), usage);
	}

	const void *mapForReading() const;
	void *mapForWriting();
	void unmap() const;

	int getHandle() const { return (int)m_handle; }

  protected:
	static int activeHandle();
	static void bindHandle(int handle);

	friend class VertexArray;

	unsigned m_handle;
	int m_size;
};

class Shader : public RefCounter {
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

class Program : public RefCounter {
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

typedef Ptr<Program> PProgram;

class Renderer {
  public:
	Renderer(const IRect &viewport);

	void pushViewMatrix();
	void popViewMatrix();
	void mulViewMatrix(const Matrix4&);
	void setViewMatrix(const Matrix4&);
	void setViewMatrix(const float2 &view_pos);
	const Matrix4 &viewMatrix() { return m_view_matrix; }

	void render();

	void addRect(const FRect &rect, const FRect &tex_rect, PTexture tex, Color color);
	void addRect(const FRect &rect, PTexture tex, Color color) {
		addRect(rect, FRect(0, 0, 1, 1), tex, color);
	}

	void addLineRect(const FRect &rect, Color color);

	// Each line is represented by two vertices
	void addLines(const float3 *pos, const Color *color, int num_lines);

	//TODO: pass ElementSource class, which can be single element, vector, pod array, whatever
	
	// Each quad is represented by 4 vertices (ordered clockwise)
	void addQuads(const float3 *pos, const float2 *tex_coord, const Color *color, int num_quads,
				  PTexture tex);

	// Each triangle is represented by 3 vertices
	void addTris(const float3 *pos, const float2 *tex_coord, const Color *color, int num_tris,
				 PTexture tex);

  protected:
	struct Element {
		Matrix4 view_matrix;
		PTexture texture;
		int first_index;
		int num_indices;
		int primitive_type;
	};

	Element &makeElement(int primitive_type, PTexture tex);

	vector<float3> m_positions;
	vector<Color> m_colors;
	vector<float2> m_tex_coords;
	vector<uint> m_indices;
	vector<Element> m_elements;

	const IRect m_viewport;
	vector<Matrix4> m_matrix_stack;
	Matrix4 m_view_matrix;
	Matrix4 m_projection_matrix;
	PProgram m_tex_program, m_flat_program;
};

void clear(Color color);

enum BlendingMode {
	bmDisabled,
	bmNormal,
};

void setBlendingMode(BlendingMode mode);
void setScissorRect(const IRect &rect);
const IRect getScissorRect();

void setScissorTest(bool is_enabled);

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

class Font : public Resource {
  public:
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

using PFont = Ptr<Font>;

class FontRenderer {
  public:
	FontRenderer(PFont font, PTexture texture, Renderer &out);

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

  private:
	void draw(const int2 &pos, Color col, const char *text) const;

	PFont m_font;
	PTexture m_texture;
	Renderer &m_renderer;
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
