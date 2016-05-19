/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#define SDL_MAIN_HANDLED

#include "fwk_gfx.h"
#include "fwk_input.h"
#include "fwk_opengl.h"
#include <SDL.h>
#include <SDL_video.h>
#include <memory.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>

namespace fwk {

static void reportSDLError(const char *func_name) {
	THROW("Error on %s: %s", func_name, SDL_GetError());
}

void initializeOpenGL();

// TODO: Something is corrupting this memory when running under emscripten
static GfxDevice *s_instance = nullptr;

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
	WindowImpl(const string &name, const int2 &size, uint flags) : flags(flags) {
		int sdl_flags = SDL_WINDOW_OPENGL;
		DASSERT(!((flags & flag_fullscreen) && (flags & flag_fullscreen_desktop)));

		int pos_x = 20, pos_y = 50;
		if(flags & flag_fullscreen)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;
		if(flags & flag_fullscreen_desktop)
			sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if(flags & flag_resizable)
			sdl_flags |= SDL_WINDOW_RESIZABLE;
		if(flags & flag_maximized) {
			sdl_flags |= SDL_WINDOW_MAXIMIZED;
			pos_x = pos_y = 0;
		}
		if(flags & flag_centered)
			pos_x = pos_y = SDL_WINDOWPOS_CENTERED;
		if(flags & flag_multisampling) {
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
		}

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
	uint flags;
};

GfxDevice::GfxDevice() : m_main_loop_function(nullptr), m_input_impl(make_unique<InputImpl>()) {
	s_instance = this;

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_Init");

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

GfxDevice::~GfxDevice() {
	m_window_impl.reset();
	s_instance = nullptr;
	SDL_Quit();
}

void GfxDevice::createWindow(const string &name, const int2 &size, uint flags) {
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = make_unique<WindowImpl>(name, size, flags);

	SDL_GL_SetSwapInterval(flags & flag_vsync ? -1 : 0);
	initializeOpenGL();
}

void GfxDevice::destroyWindow() { m_window_impl.reset(); }

void GfxDevice::setWindowSize(const int2 &size) {
	if(m_window_impl)
		SDL_SetWindowSize(m_window_impl->window, size.x, size.y);
}

void GfxDevice::setWindowFullscreen(uint flags) {
	DASSERT(flags == 0 || flags == flag_fullscreen || flags == flag_fullscreen_desktop);

	if(m_window_impl) {
		uint sdl_flags = flags & flag_fullscreen
							 ? SDL_WINDOW_FULLSCREEN
							 : flags & flag_fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
		SDL_SetWindowFullscreen(m_window_impl->window, sdl_flags);
		uint mask = flag_fullscreen | flag_fullscreen_desktop;
		m_window_impl->flags = (m_window_impl->flags & ~mask) | (mask & flags);
	}
}

uint GfxDevice::windowFlags() const { return m_window_impl ? m_window_impl->flags : 0; }

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
	DASSERT(inst.m_main_loop_function);
	double time = getTime();
	inst.m_frame_time = inst.m_last_time < 0.0 ? 0.0 : time - inst.m_last_time;
	inst.m_last_time = time;

	inst.pollEvents();
	inst.m_main_loop_function(inst);
}
#endif

void GfxDevice::runMainLoop(MainLoopFunction function) {
	m_main_loop_function = function;
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(emscriptenCallback, 0, 1);
#else
	while(pollEvents()) {
		double time = getTime();
		m_frame_time = m_last_time < 0.0 ? 0.0 : time - m_last_time;
		m_last_time = time;

		if(!function(*this))
			break;
		SDL_GL_SwapWindow(m_window_impl->window);
	}
#endif
	m_main_loop_function = nullptr;
}

void GfxDevice::grabMouse(bool grab) {
	if(m_window_impl)
		SDL_SetWindowGrab(m_window_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void GfxDevice::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

const InputState &GfxDevice::inputState() const { return m_input_impl->state; }

const vector<InputEvent> &GfxDevice::inputEvents() const { return m_input_impl->events; }

string GfxDevice::extensions() const { return (const char *)glGetString(GL_EXTENSIONS); }

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
