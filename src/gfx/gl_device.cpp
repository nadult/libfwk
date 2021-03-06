// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/gfx/gl_device.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"
#include "fwk/math/box.h"
#include "fwk/sys/input.h"
#include "fwk/sys/thread.h"
#include <SDL.h>
#include <SDL_video.h>
#include <memory.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>

namespace fwk {

static const DebugFlagsCheck debug_flags_check;

static void reportSDLError(const char *func_name) {
	FATAL("Error on %s: %s", func_name, SDL_GetError());
}

void initializeGl(GlProfile);
void initializeGlProgramFuncs();

// TODO: Something is corrupting this memory when running under emscripten
static GlDevice *s_instance = nullptr;

static volatile int s_gl_thread_id = -1;

bool onGlThread() { return threadId() == s_gl_thread_id; }

void assertGlThread() {
	if(s_gl_thread_id == -1)
		FATAL("No OpenGL device present");
	if(threadId() != s_gl_thread_id)
		FATAL("Calling OpenGL-related function on a wrong thread");
}

bool GlDevice::isPresent() { return !!s_instance; }

GlDevice &GlDevice::instance() {
	DASSERT(s_instance);
	return *s_instance;
}

struct GlDevice::InputImpl {
	InputState state;
	vector<InputEvent> events;
	SDLKeyMap key_map;
};

static Maybe<GlFormat> sdlPixelFormat(uint pf) {
	switch(pf) {
#define CASE(sdl_type, id)                                                                         \
	case SDL_PIXELFORMAT_##sdl_type:                                                               \
		return GlFormat::id;
		CASE(RGBA8888, rgba)
		CASE(BGRA8888, bgra)
#undef CASE
	default:
		break;
	}
	return none;
}

struct GlDevice::WindowImpl {
  public:
	WindowImpl(const string &name, const int2 &size, Flags flags, GlProfile gl_profile,
			   double ogl_ver)
		: flags(flags) {
		int sdl_flags = SDL_WINDOW_OPENGL;
		DASSERT(!((flags & Opt::fullscreen) && (flags & Opt::fullscreen_desktop)));

		int pos_x = 20, pos_y = 50;
		if(flags & Opt::fullscreen)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;
		if(flags & Opt::fullscreen_desktop)
			sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if(flags & Opt::resizable)
			sdl_flags |= SDL_WINDOW_RESIZABLE;
		if(flags & Opt::maximized) {
			sdl_flags |= SDL_WINDOW_MAXIMIZED;
			pos_x = pos_y = 0;
		}
		if(flags & Opt::centered)
			pos_x = pos_y = SDL_WINDOWPOS_CENTERED;
		if(flags & Opt::multisampling) {
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
		}

		int ver_major = int(ogl_ver);
		int ver_minor = int((ogl_ver - float(ver_major)) * 10.0);
		int profile = gl_profile == GlProfile::compatibility
						  ? SDL_GL_CONTEXT_PROFILE_COMPATIBILITY
						  : gl_profile == GlProfile::es ? SDL_GL_CONTEXT_PROFILE_ES
														: SDL_GL_CONTEXT_PROFILE_CORE;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ver_major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ver_minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

		window = SDL_CreateWindow(name.c_str(), pos_x, pos_y, size.x, size.y, sdl_flags);
		if(!window)
			reportSDLError("SDL_CreateWindow");
		if(!(gl_context = SDL_GL_CreateContext(window))) {
			SDL_DestroyWindow(window);
			reportSDLError("SDL_GL_CreateContext");
		}

		pixel_format = sdlPixelFormat(SDL_GetWindowPixelFormat(window));
	}

	~WindowImpl() {
		program_cache.clear();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}

	SDL_Window *window;
	SDL_GLContext gl_context;
	Maybe<GlFormat> pixel_format;
	HashMap<string, PProgram> program_cache;
	Flags flags;
};

GlDevice::GlDevice(DebugFlagsCheck check) {
	m_input_impl.emplace();
	ASSERT("Only one instance of GlDevice can be created at a time" && !s_instance);
	if(check.opengl != debug_flags_check.opengl)
		FATAL("If libfwk is compiled with FWK_CHECK_OPENGL then target program must be compiled "
			  "with this flag as well (and vice-versa)");

	s_instance = this;
	s_gl_thread_id = threadId();

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_Init");

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

GlDevice::~GlDevice() {
	PASSERT_GL_THREAD();
	m_window_impl.reset();
	s_instance = nullptr;
	s_gl_thread_id = -1;
	SDL_Quit();
}

void GlDevice::createWindow(const string &name, const int2 &size, Flags flags, GlProfile gl_profile,
							double ogl_ver) {
#ifdef FWK_PLATFORM_HTML
	if(gl_profile != GlProfile::es) {
		gl_profile = GlProfile::es;
		ogl_ver = 3;
	}
#endif

	assertGlThread();
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = {name, size, flags, gl_profile, ogl_ver};

	SDL_GL_SetSwapInterval(flags & Opt::vsync ? -1 : 0);
	initializeGl(gl_profile);
	initializeGlProgramFuncs();
	if(flags & Opt::full_debug)
		gl_debug_flags = all<GlDebug>;

	if(flags & Opt::opengl_debug_handler)
		installGlDebugHandler();
}

void GlDevice::destroyWindow() { m_window_impl.reset(); }

void GlDevice::setWindowSize(const int2 &size) {
	if(m_window_impl)
		SDL_SetWindowSize(m_window_impl->window, size.x, size.y);
}

void GlDevice::setWindowRect(IRect rect) {
	if(m_window_impl) {
		SDL_SetWindowSize(m_window_impl->window, rect.width(), rect.height());
		SDL_SetWindowPosition(m_window_impl->window, rect.x(), rect.y());
	}
}

IRect GlDevice::windowRect() const {
	int2 pos, size;
	if(m_window_impl) {
		size = windowSize();
		SDL_GetWindowPosition(m_window_impl->window, &pos.x, &pos.y);
	}
	return IRect(pos, pos + size);
}

EnumMap<RectSide, int> GlDevice::windowBorder() const {
	int top = 0, bottom = 0, left = 0, right = 0;
	SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
	return {{right, top, left, bottom}};
}

Maybe<GlFormat> GlDevice::pixelFormat() const {
	return m_window_impl ? m_window_impl->pixel_format : none;
}

void GlDevice::setWindowFullscreen(Flags flags) {
	DASSERT(!flags || flags == Opt::fullscreen || flags == Opt::fullscreen_desktop);

	if(m_window_impl) {
		uint sdl_flags = flags & Opt::fullscreen
							 ? SDL_WINDOW_FULLSCREEN
							 : flags & Opt::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
		SDL_SetWindowFullscreen(m_window_impl->window, sdl_flags);
		auto mask = Opt::fullscreen | Opt::fullscreen_desktop;
		m_window_impl->flags = (m_window_impl->flags & ~mask) | (mask & flags);
	}
}

auto GlDevice::windowFlags() const -> Flags {
	return m_window_impl ? m_window_impl->flags : Flags();
}

int2 GlDevice::windowSize() const {
	int2 out;
	if(m_window_impl)
		SDL_GetWindowSize(m_window_impl->window, &out.x, &out.y);
	return out;
}

void GlDevice::printDeviceInfo() {
	int max_tex_size;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);

	printf("Opengl info\n"
		   "Vendor: %s\nRenderer: %s\n"
		   "Maximum texture size: %d\n",
		   vendor, renderer, max_tex_size);
}

int GlDevice::swapInterval() { return SDL_GL_GetSwapInterval(); }
void GlDevice::setSwapInterval(int value) { SDL_GL_SetSwapInterval(value); }

bool GlDevice::pollEvents() {
	m_input_impl->events =
		m_input_impl->state.pollEvents(m_input_impl->key_map, m_window_impl->window);
	for(const auto &event : m_input_impl->events)
		if(event.type() == InputEventType::quit)
			return false;
	return true;
}

#ifdef __EMSCRIPTEN__
void GlDevice::emscriptenCallback() {
	GlDevice &inst = instance();
	DASSERT(!inst.m_main_loop_stack.empty());
	auto stack_top = inst.m_main_loop_stack.front();
	double time = getTime();
	inst.m_frame_time = inst.m_last_time < 0.0 ? 0.0 : time - inst.m_last_time;
	inst.m_last_time = time;

	inst.pollEvents();
	if(!stack_top.first(inst, stack_top.second))
		emscripten_cancel_main_loop();
}
#endif

void GlDevice::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	PASSERT_GL_THREAD();
	ASSERT(main_loop_func);
	m_main_loop_stack.emplace_back(main_loop_func, arg);
#ifdef __EMSCRIPTEN__
	if(m_main_loop_stack.size() > 1)
		FATAL("Emscripten doesn't support multiple main loops");
	emscripten_set_main_loop(emscriptenCallback, 0, 1);
#else
	while(pollEvents()) {
		double time = getTime();
		m_frame_time = m_last_time < 0.0 ? 0.0 : time - m_last_time;
		m_last_time = time;

		if(!main_loop_func(*this, arg))
			break;
		SDL_GL_SwapWindow(m_window_impl->window);
	}
#endif
	m_main_loop_stack.pop_back();
}

void GlDevice::grabMouse(bool grab) {
	if(m_window_impl)
		SDL_SetWindowGrab(m_window_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void GlDevice::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

const InputState &GlDevice::inputState() const { return m_input_impl->state; }

const vector<InputEvent> &GlDevice::inputEvents() const { return m_input_impl->events; }

string GlDevice::extensions() const { return (const char *)glGetString(GL_EXTENSIONS); }

string GlDevice::clipboardText() const {
	auto ret = SDL_GetClipboardText();
	return ret ? string(ret) : string();
}

void GlDevice::setClipboardText(ZStr str) { SDL_SetClipboardText(str.c_str()); }

PProgram GlDevice::cacheFindProgram(Str name) const {
	if(!m_window_impl)
		return {};
	auto &cache = m_window_impl->program_cache;
	auto it = cache.find(name);
	return it ? it->value : PProgram();
}

void GlDevice::cacheAddProgram(Str name, PProgram program) {
	DASSERT(program);
	if(m_window_impl)
		m_window_impl->program_cache[name] = program;
}
}
