// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/sys/platform.h"
#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#endif

#include "fwk/gfx/gl_device.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx/opengl.h"
#include "fwk/hash_map.h"
#include "fwk/math/box.h"
#include "fwk/sys/input.h"
#include "fwk/sys/thread.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
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

struct GlDevice::WindowImpl {
  public:
	WindowImpl(ZStr title, IRect rect, GlDeviceConfig config) : config(config) {
		int sdl_flags = SDL_WINDOW_OPENGL;
		auto flags = config.flags;
		DASSERT(!((flags & Opt::fullscreen) && (flags & Opt::fullscreen_desktop)));

		int2 pos = rect.min(), size = rect.size();
		if(flags & Opt::fullscreen)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;
		if(flags & Opt::fullscreen_desktop)
			sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if(flags & Opt::resizable)
			sdl_flags |= SDL_WINDOW_RESIZABLE;
		if(flags & Opt::allow_hidpi)
			sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
		if(flags & Opt::maximized)
			sdl_flags |= SDL_WINDOW_MAXIMIZED;
		if(flags & Opt::centered)
			pos.x = pos.y = SDL_WINDOWPOS_CENTERED;
		if(config.multisampling) {
			DASSERT(*config.multisampling >= 1 && *config.multisampling <= 64);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, *config.multisampling);
		}

		int ver_major = int(config.version);
		int ver_minor = int((config.version - float(ver_major)) * 10.0);
		int profile = config.profile == GlProfile::compatibility ?
														  SDL_GL_CONTEXT_PROFILE_COMPATIBILITY :
					  config.profile == GlProfile::es ? SDL_GL_CONTEXT_PROFILE_ES :
														  SDL_GL_CONTEXT_PROFILE_CORE;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ver_major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ver_minor);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

		window = SDL_CreateWindow(title.c_str(), pos.x, pos.y, size.x, size.y, sdl_flags);

		if(!window)
			reportSDLError("SDL_CreateWindow");

		if(!(gl_context = SDL_GL_CreateContext(window))) {
			SDL_DestroyWindow(window);
			reportSDLError("SDL_GL_CreateContext");
		}
	}

	~WindowImpl() {
		program_cache.clear();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}

	SDL_Window *window;
	SDL_GLContext gl_context;
	HashMap<string, PProgram> program_cache;
	GlDeviceConfig config;
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

vector<IRect> GlDevice::displayRects() const {
	int count = SDL_GetNumVideoDisplays();
	vector<IRect> out(count);
	for(int idx = 0; idx < count; idx++) {
		SDL_Rect rect;
		if(SDL_GetDisplayBounds(idx, &rect) != 0)
			FWK_FATAL("Error in SDL_GetDisplayBounds");
		out[idx] = {rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
	}
	return out;
}

vector<float> GlDevice::displayDpiScales() const {
	int count = SDL_GetNumVideoDisplays();
	vector<float> out(count);
	for(int idx = 0; idx < count; idx++) {
		// TODO: make sure that it's correct for all different platforms
		const float default_dpi = 96;
		float current_dpi = default_dpi;
		SDL_GetDisplayDPI(idx, nullptr, &current_dpi, nullptr);
		out[idx] = current_dpi / default_dpi;
	}
	return out;
}

IRect GlDevice::sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
								   float minimum_overlap) {
	if(!display_rects)
		return window_rect;
	IRect nonempty_rect{window_rect.min(), window_rect.min() + max(int2(1), window_rect.size())};

	float area = window_rect.surfaceArea(), overlap = 0.0;
	for(auto &display_rect : display_rects) {
		auto isect = nonempty_rect.intersection(display_rect);
		if(isect)
			overlap += isect->surfaceArea();
	}

	if(overlap / area < minimum_overlap) {
		auto first_display = display_rects[0];
		int2 new_window_pos = first_display.center() - window_rect.size() / 2;
		return IRect(new_window_pos, new_window_pos + window_rect.size());
	}
	return window_rect;
}

void GlDevice::createWindow(ZStr title, IRect rect, Config config) {
#ifdef FWK_PLATFORM_HTML
	if(config.profile != GlProfile::es) {
		config.profile = GlProfile::es;
		config.version = 3;
	}
#endif

	assertGlThread();
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = {title, rect, config};

	SDL_GL_SetSwapInterval(config.flags & Opt::vsync ? -1 : 0);
	initializeGl(config.profile);
	initializeGlProgramFuncs();
	if(config.flags & Opt::full_debug)
		gl_debug_flags = all<GlDebug>;
	if(config.flags & Opt::opengl_debug_handler)
		installGlDebugHandler();
}

void GlDevice::destroyWindow() { m_window_impl.reset(); }

void GlDevice::setWindowTitle(ZStr title) {
	SDL_SetWindowTitle(m_window_impl->window, title.c_str());
}

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
		if(platform == Platform::linux) {
			// Workaround for SDL2 bug on linux platforms
			// (TODO: find out exactly where it happens?)
			int top, left, bottom, right;
			SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
			pos -= int2{left, top};
		}
	}
	return IRect(pos, pos + size);
}

#ifdef FWK_PLATFORM_WINDOWS
Pair<int2> windowNonClientBorders(HWND hwnd) {
	RECT non_client = {0, 0, 0, 0}, window = {0, 0, 0, 0}, client = {0, 0, 0, 0};
	POINT client_to_screen = {0, 0};

	if(GetWindowRect(hwnd, &window) && GetClientRect(hwnd, &client) &&
	   ClientToScreen(hwnd, &client_to_screen)) {
		return {{client_to_screen.x - window.left, client_to_screen.y - window.top},
				{window.right - client_to_screen.x - client.right,
				 window.bottom - client_to_screen.y - client.bottom}};
	}
	return {};
}
#endif

IRect GlDevice::restoredWindowRect() const {
	IRect window_rect = windowRect();

#ifdef FWK_PLATFORM_WINDOWS
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_window_impl->window, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;
	WINDOWPLACEMENT placement;
	GetWindowPlacement(hwnd, &placement);
	IRect out(placement.rcNormalPosition.left, placement.rcNormalPosition.top,
			  placement.rcNormalPosition.right, placement.rcNormalPosition.bottom);
	auto borders = windowNonClientBorders(hwnd);
	return {out.min() + borders.first, out.max() - borders.second};
#else
	return window_rect;
#endif
}

EnumMap<RectSide, int> GlDevice::windowBorder() const {
	int top = 0, bottom = 0, left = 0, right = 0;
	SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
	return {{right, top, left, bottom}};
}

void GlDevice::setWindowFullscreen(Flags flags) {
	DASSERT(!flags || isOneOf(flags, Opt::fullscreen, Opt::fullscreen_desktop));

	if(m_window_impl) {
		uint sdl_flags = flags == Opt::fullscreen		  ? SDL_WINDOW_FULLSCREEN :
						 flags == Opt::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP :
															  0;
		SDL_SetWindowFullscreen(m_window_impl->window, sdl_flags);
		auto mask = Opt::fullscreen | Opt::fullscreen_desktop;
		m_window_impl->config.flags = (m_window_impl->config.flags & ~mask) | (mask & flags);
	}
}

bool GlDevice::isWindowFullscreen() const {
	return windowFlags() & (Opt::fullscreen | Opt::fullscreen_desktop);
}

bool GlDevice::isWindowMaximized() const {
	auto flags = SDL_GetWindowFlags(m_window_impl->window);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

int GlDevice::windowDisplayIndex() const {
	return SDL_GetWindowDisplayIndex(m_window_impl->window);
}

float GlDevice::windowDpiScale() const {
	auto display_dpis = displayDpiScales();
	auto display_index = windowDisplayIndex();
	return display_dpis.inRange(display_index) ? display_dpis[display_index] : 1.0f;
}

auto GlDevice::windowFlags() const -> Flags {
	return m_window_impl ? m_window_impl->config.flags : Flags();
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
