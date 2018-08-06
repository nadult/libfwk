// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/gfx/gfx_device.h"

#include "fwk/gfx/color.h"
#include "fwk/gfx/opengl.h"
#include "fwk/sys/input.h"
#include <SDL.h>
#include <SDL_video.h>
#include <memory.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>

namespace fwk {

static void reportSDLError(const char *func_name) {
	FATAL("Error on %s: %s", func_name, SDL_GetError());
}

void initializeOpenGL(OpenglProfile);

// TODO: Something is corrupting this memory when running under emscripten
static GfxDevice *s_instance = nullptr;

static volatile int s_gfx_thread_id = -1;

bool onGfxThread() { return threadId() == s_gfx_thread_id; }

void assertGfxThread() {
	if(s_gfx_thread_id != -1 && threadId() != s_gfx_thread_id)
		ASSERT_FAILED("Calling gfx-related function on a wrong thread");
}

GfxDevice &GfxDevice::instance() {
	DASSERT(s_instance);
	return *s_instance;
}

struct GfxDevice::InputImpl {
	InputState state;
	vector<InputEvent> events;
	SDLKeyMap key_map;
};

struct GfxDevice::WindowImpl {
  public:
	WindowImpl(const string &name, const int2 &size, Flags flags, double ogl_ver) : flags(flags) {
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

		bool opengl_es = (bool)(flags & Opt::opengl_es_profile);
		int ver_major = int(ogl_ver);
		int ver_minor = int((ogl_ver - float(ver_major)) * 10.0);
		int profile = opengl_es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE;

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
	}
	~WindowImpl() {
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}

	SDL_Window *window;
	SDL_GLContext gl_context;
	Flags flags;
};

GfxDevice::GfxDevice() : m_input_impl(uniquePtr<InputImpl>()) {
	ASSERT("Only one instance of GfxDevice can be created at a time" && !s_instance);
	s_instance = this;
	s_gfx_thread_id = threadId();

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_Init");

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

GfxDevice::~GfxDevice() {
	PASSERT_GFX_THREAD();
	m_window_impl.reset();
	s_instance = nullptr;
	s_gfx_thread_id = -1;
	SDL_Quit();
}

void GfxDevice::createWindow(const string &name, const int2 &size, Flags flags, double ogl_ver) {
	assertGfxThread();
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = uniquePtr<WindowImpl>(name, size, flags, ogl_ver);

	SDL_GL_SetSwapInterval(flags & Opt::vsync ? -1 : 0);
	initializeOpenGL(flags & Opt::opengl_es_profile ? OpenglProfile::es : OpenglProfile::core);
	if(flags & Opt::opengl_debug_handler)
		installOpenglDebugHandler();
}

void GfxDevice::destroyWindow() { m_window_impl.reset(); }

void GfxDevice::setWindowSize(const int2 &size) {
	if(m_window_impl)
		SDL_SetWindowSize(m_window_impl->window, size.x, size.y);
}

void GfxDevice::setWindowFullscreen(Flags flags) {
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

auto GfxDevice::windowFlags() const -> Flags {
	return m_window_impl ? m_window_impl->flags : Flags();
}

int2 GfxDevice::windowSize() const {
	int2 out;
	if(m_window_impl)
		SDL_GetWindowSize(m_window_impl->window, &out.x, &out.y);
	return out;
}

void GfxDevice::printDeviceInfo() {
	int max_tex_size;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);

	printf("Opengl info\n"
		   "Vendor: %s\nRenderer: %s\n"
		   "Maximum texture size: %d\n",
		   vendor, renderer, max_tex_size);
}

bool GfxDevice::pollEvents() {
	m_input_impl->events = m_input_impl->state.pollEvents(m_input_impl->key_map);
	for(const auto &event : m_input_impl->events)
		if(event.type() == InputEvent::quit)
			return false;
	return true;
}

#ifdef __EMSCRIPTEN__
void GfxDevice::emscriptenCallback() {
	GfxDevice &inst = instance();
	DASSERT(!inst.m_main_loop_stack.empty());
	auto stack_top = inst.m_main_loop_stack.front();
	double time = getTime();
	inst.m_frame_time = inst.m_last_time < 0.0 ? 0.0 : time - inst.m_last_time;
	inst.m_last_time = time;

	inst.pollEvents();
	stack_top.first(inst, stack_top.second);
}
#endif

void GfxDevice::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	PASSERT_GFX_THREAD();
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

void GfxDevice::grabMouse(bool grab) {
	if(m_window_impl)
		SDL_SetWindowGrab(m_window_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void GfxDevice::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

const InputState &GfxDevice::inputState() const { return m_input_impl->state; }

const vector<InputEvent> &GfxDevice::inputEvents() const { return m_input_impl->events; }

string GfxDevice::extensions() const { return (const char *)glGetString(GL_EXTENSIONS); }

string GfxDevice::clipboardText() const {
	auto ret = SDL_GetClipboardText();
	return ret ? string(ret) : string();
}

void GfxDevice::setClipboardText(ZStr str) { SDL_SetClipboardText(str.c_str()); }

void GfxDevice::clearColor(FColor col) {
	glClearColor(col.r, col.g, col.b, col.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GfxDevice::clearDepth(float depth_value) {
	glClearDepth(depth_value);
	glDepthMask(1);
	glClear(GL_DEPTH_BUFFER_BIT);
}
}
