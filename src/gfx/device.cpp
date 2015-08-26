/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#define SDL_MAIN_HANDLED

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <memory.h>
#include <SDL_video.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>

namespace fwk {

DEFINE_ENUM(PrimitiveType, "points", "lines", "triangles", "triangle_strip");

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

GfxDevice::GfxDevice()
	: m_main_loop_function(nullptr) /*,m_press_delay(0.2), m_clock(0)*/
{
	s_instance = this;

	struct Pair {
		int key, sdl_key;
	} pairs[] = {
#define PAIR(input_key, sdl_key)                                                                   \
	{ InputKey::input_key, SDLK_##sdl_key }
		PAIR(space, SPACE), PAIR(esc, ESCAPE), PAIR(f1, F1), PAIR(f2, F2), PAIR(f3, F3),
		PAIR(f4, F4), PAIR(f5, F5), PAIR(f6, F6), PAIR(f7, F7), PAIR(f8, F8), PAIR(f9, F9),
		PAIR(f10, F10), PAIR(f11, F11), PAIR(f12, F12), PAIR(up, UP), PAIR(down, DOWN),
		PAIR(left, LEFT), PAIR(right, RIGHT), PAIR(lshift, LSHIFT), PAIR(rshift, RSHIFT),
		PAIR(lctrl, LCTRL), PAIR(rctrl, RCTRL), PAIR(lalt, LALT), PAIR(ralt, RALT), PAIR(tab, TAB),
		PAIR(enter, RETURN), PAIR(backspace, BACKSPACE), PAIR(insert, INSERT), PAIR(del, DELETE),
		PAIR(pageup, PAGEUP), PAIR(pagedown, PAGEDOWN), PAIR(home, HOME), PAIR(end, END),
		/*	PAIR(kp_0, KP_0),
			PAIR(kp_1, KP_1),
			PAIR(kp_2, KP_2),
			PAIR(kp_3, KP_3),
			PAIR(kp_4, KP_4),
			PAIR(kp_5, KP_5),
			PAIR(kp_6, KP_6),
			PAIR(kp_7, KP_7),
			PAIR(kp_8, KP_8),
			PAIR(kp_9, KP_9),*/
		PAIR(kp_0, KP_0), PAIR(kp_1, KP_1), PAIR(kp_2, KP_2), PAIR(kp_3, KP_3), PAIR(kp_4, KP_4),
		PAIR(kp_5, KP_5), PAIR(kp_6, KP_6), PAIR(kp_7, KP_7), PAIR(kp_8, KP_8), PAIR(kp_9, KP_9),
		PAIR(kp_divide, KP_DIVIDE), PAIR(kp_multiply, KP_MULTIPLY), PAIR(kp_subtract, KP_MINUS),
		PAIR(kp_add, KP_PLUS),
		//	PAIR(kp_decimal, KP_DECIMAL),
		PAIR(kp_enter, KP_ENTER), PAIR(kp_period, KP_PERIOD),
#undef PAIR
	};

	for(int n = 0; n < arraySize(pairs); n++) {
		m_key_map[pairs[n].key] = pairs[n].sdl_key;
		m_inv_map[pairs[n].sdl_key] = pairs[n].key;
	}

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_Init");

	memset(&m_input_state, 0, sizeof(m_input_state));
	m_is_input_state_initialized = false;

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

int GfxDevice::translateToSDL(int key_code) const {
	DASSERT(key_code >= 0);
	if(key_code >= 0 && key_code <= 255)
		return key_code;
	auto it = m_key_map.find(key_code);
	DASSERT(it != m_key_map.end());
	return it->second;
}

int GfxDevice::translateFromSDL(int key_code) const {
	DASSERT(key_code >= 0);
	if(key_code >= 0 && key_code <= 255)
		return key_code;
	auto it = m_inv_map.find(key_code);
	return it == m_inv_map.end() ? -1 : it->second;
}

GfxDevice::~GfxDevice() {
	m_window_impl.reset();
	s_instance = nullptr;
	SDL_Quit();
}

struct GfxDevice::WindowImpl {
  public:
	WindowImpl(const string &name, const int2 &size, uint flags) : flags(flags) {
		int sdl_flags = SDL_WINDOW_OPENGL;
		DASSERT(!((flags & flag_fullscreen) && (flags & flag_fullscreen_desktop)));

		if(flags & flag_fullscreen)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;
		if(flags & flag_fullscreen_desktop)
			sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if(flags & flag_resizable)
			sdl_flags |= SDL_WINDOW_RESIZABLE;
		if(flags & flag_multisampling) {
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
		}
		int window_pos = flags & flag_centered ? SDL_WINDOWPOS_CENTERED : 0;
		window = SDL_CreateWindow(name.c_str(), window_pos, window_pos, size.x, size.y, sdl_flags);
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
	SDL_Event event;

	vector<InputEvent> events;

	if(!m_is_input_state_initialized) {
		SDL_GetMouseState(&m_input_state.mouse_pos.x, &m_input_state.mouse_pos.y);
		m_is_input_state_initialized = true;
	} else {
		for(auto &key_state : m_input_state.keys)
			if(key_state.second >= 0)
				key_state.second++;

		m_input_state.keys.resize(
			std::remove_if(m_input_state.keys.begin(), m_input_state.keys.end(),
						   [](const pair<int, int> &state) { return state.second == -1; }) -
			m_input_state.keys.begin());

		for(int n = 0; n < InputButton::count; n++) {
			auto &state = m_input_state.mouse_buttons[n];
			if(state == 1)
				state = 2;
			else if(state == -1)
				state = 0;
		}
	}
	m_input_state.mouse_move = int2(0, 0);
	m_input_state.mouse_wheel = 0;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_KEYDOWN: {
			int key_id = translateFromSDL(event.key.keysym.sym);
			if(key_id == -1)
				break;

			bool is_pressed = false;
			for(const auto &key_state : m_input_state.keys)
				if(key_state.first == key_id)
					is_pressed = true;
			if(!is_pressed) {
				m_input_state.keys.emplace_back(key_id, 0);
				events.emplace_back(InputEvent::key_down, key_id, 0);
			}
			break;
		}
		case SDL_KEYUP: {
			int key_id = translateFromSDL(event.key.keysym.sym);
			if(key_id == -1)
				break;

			events.emplace_back(InputEvent::key_up, key_id, 0);
			for(auto &key_state : m_input_state.keys)
				if(key_state.first == key_id)
					key_state.second = -1;
			break;
		}
		case SDL_MOUSEMOTION:
			m_input_state.mouse_move += int2(event.motion.xrel, event.motion.yrel);
			break;
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN: {
			bool is_down = event.type == SDL_MOUSEBUTTONDOWN;

			// TODO: fix formatting
			int button_id =
				event.button.button == SDL_BUTTON_LEFT
					? InputButton::left
					: event.button.button == SDL_BUTTON_RIGHT
						  ? InputButton::right
						  : event.button.button == SDL_BUTTON_MIDDLE ? InputButton::middle : -1;
			if(button_id != -1) {
				m_input_state.mouse_buttons[button_id] = is_down ? 1 : -1;
				events.emplace_back(is_down ? InputEvent::mouse_button_down
											: InputEvent::mouse_button_up,
									InputButton::Type(button_id));
			}
			break;
		}
		case SDL_MOUSEWHEEL:
			m_input_state.mouse_wheel = event.wheel.y;
			break;
		case SDL_QUIT:
			events.emplace_back(InputEvent::quit);
			return false;
		}
	}

	SDL_GetMouseState(&m_input_state.mouse_pos.x, &m_input_state.mouse_pos.y);

	m_input_events = std::move(events);
	int modifiers = 0;

	for(const auto &key_state : m_input_state.keys) {
		if(key_state.second >= 1)
			m_input_events.emplace_back(InputEvent::key_pressed, key_state.first, key_state.second);
		if(key_state.second >= 0) {
			modifiers |= (key_state.first == InputKey::lshift ? InputEvent::mod_lshift : 0) |
						 (key_state.first == InputKey::rshift ? InputEvent::mod_rshift : 0) |
						 (key_state.first == InputKey::lctrl ? InputEvent::mod_lctrl : 0) |
						 (key_state.first == InputKey::lalt ? InputEvent::mod_lalt : 0);
		}
	}

	for(int n = 0; n < InputButton::count; n++)
		if(m_input_state.mouse_buttons[n] == 2)
			m_input_events.emplace_back(InputEvent::mouse_button_pressed, InputButton::Type(n));
	m_input_events.emplace_back(InputEvent::mouse_over);

	for(auto &event : m_input_events)
		event.init(modifiers, m_input_state.mouse_pos, m_input_state.mouse_move,
				   m_input_state.mouse_wheel);

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

void GfxDevice::grabMouse(bool grab) { THROW("Writeme"); }

void GfxDevice::showCursor(bool flag) { THROW("Writeme"); }

/*
InputState GfxDevice::inputState() {
	InputState out;

		const int2 mouse_pos = getMousePos(), mouse_move = getMouseMove();

		out.push_back({InputEvent::mouse_over, 0, mouse_move});
		if(getMouseWheelMove())
			out.push_back({InputEvent::mouse_wheel, 0, int2(getMouseWheelMove(), 0)});

		for(int n = 0; n < InputKey::count; n++) {
			if(isKeyDown(n))
				out.push_back({InputEvent::key_down, n, 0});
			if(isKeyUp(n))
				out.push_back({InputEvent::key_up, n, 0});
			if(isKeyPressed(n))
				out.push_back({InputEvent::key_pressed, n, 0});
			if(isKeyDownAuto(n, 1) && !isKeyDown(n))
				out.push_back({InputEvent::key_down_auto, n, m_impl->clock});
		}

		for(int mk = 0; mk < 3; mk++) {
			if(isMouseKeyDown(mk))
				out.push_back({InputEvent::mouse_key_down, mk, mouse_move});
			if(isMouseKeyUp(mk))
				out.push_back({InputEvent::mouse_key_up, mk, mouse_move});
			if(isMouseKeyPressed(mk))
				out.push_back({InputEvent::mouse_key_pressed, mk, mouse_move});
		}

		for(int n = 0; n < (int)out.size(); n++)
			out[n].init(modifiers, mouse_pos);

	return out;
}*/

string GfxDevice::extensions() const { return (const char *)glGetString(GL_EXTENSIONS); }

void GfxDevice::clearColor(Color color) {
	float4 col = color;
	glClearColor(col.x, col.y, col.z, col.w);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GfxDevice::clearDepth(float depth_value) {
	glClearDepth(depth_value);
	glDepthMask(1);
	glClear(GL_DEPTH_BUFFER_BIT);
}

}
