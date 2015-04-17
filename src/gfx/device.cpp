/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

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

void loadExtensions();

GfxDevice &GfxDevice::instance() {
	static GfxDevice s_instance;
	return s_instance;
}

GfxDevice::GfxDevice()
	: m_main_loop_function(nullptr), m_last_time(-1.0), m_press_delay(0.2), m_clock(0) {

	struct Pair {
		int key, sdl_key;
	} pairs[] = {
#define PAIR(input_key, sdl_key)                                                                   \
	{ InputKey::input_key, SDLK_##sdl_key }
		  PAIR(space, SPACE),		   PAIR(esc, ESCAPE),		   PAIR(f1, F1),
		  PAIR(f2, F2),				   PAIR(f3, F3),			   PAIR(f4, F4),
		  PAIR(f5, F5),				   PAIR(f6, F6),			   PAIR(f7, F7),
		  PAIR(f8, F8),				   PAIR(f9, F9),			   PAIR(f10, F10),
		  PAIR(f11, F11),			   PAIR(f12, F12),			   PAIR(up, UP),
		  PAIR(down, DOWN),			   PAIR(left, LEFT),		   PAIR(right, RIGHT),
		  PAIR(lshift, LSHIFT),		   PAIR(rshift, RSHIFT),	   PAIR(lctrl, LCTRL),
		  PAIR(rctrl, RCTRL),		   PAIR(lalt, LALT),		   PAIR(ralt, RALT),
		  PAIR(tab, TAB),			   PAIR(enter, RETURN),		   PAIR(backspace, BACKSPACE),
		  PAIR(insert, INSERT),		   PAIR(del, DELETE),		   PAIR(pageup, PAGEUP),
		  PAIR(pagedown, PAGEDOWN),	PAIR(home, HOME),		   PAIR(end, END),
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
		  PAIR(kp_0, KP0),			   PAIR(kp_1, KP1),			   PAIR(kp_2, KP2),
		  PAIR(kp_3, KP3),			   PAIR(kp_4, KP4),			   PAIR(kp_5, KP5),
		  PAIR(kp_6, KP6),			   PAIR(kp_7, KP7),			   PAIR(kp_8, KP8),
		  PAIR(kp_9, KP9),			   PAIR(kp_divide, KP_DIVIDE), PAIR(kp_multiply, KP_MULTIPLY),
		  PAIR(kp_subtract, KP_MINUS), PAIR(kp_add, KP_PLUS),
		  //	PAIR(kp_decimal, KP_DECIMAL),
		  PAIR(kp_enter, KP_ENTER),	PAIR(kp_period, KP_PERIOD),
#undef PAIR
	  };

	for(int n = 0; n < arraySize(pairs); n++) {
		m_key_map[pairs[n].key] = pairs[n].sdl_key;
		m_inv_map[pairs[n].sdl_key] = pairs[n].key;
	}

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
		THROW("Error while initializing SDL");
	//	SDL_GL_SetSwapInterval(1);

	memset(&m_input_state, 0, sizeof(m_input_state));
	m_is_input_state_initialized = false;
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
	DASSERT(it != m_inv_map.end());
	return it->second;
}

GfxDevice::~GfxDevice() { SDL_Quit(); }

void GfxDevice::createWindow(int2 size, bool full) {
	SDL_Surface *surface = SDL_SetVideoMode(size.x, size.y, 32, SDL_OPENGL | SDL_DOUBLEBUF);
	if(!surface)
		THROW("Error while creating SDL window");
	//	if(SDL_CreateWindow("foobar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.x,
	// size.y,
	//						SDL_WINDOW_OPENGL) != 0)
	//		THROW("Error while creating SDL window");

	//	SDL_GL_SetSwapInterval(1);

	loadExtensions();
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
//	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
//	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
//	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(0);

	glViewport(0, 0, size.x, size.y);
	setBlendingMode(bmNormal);
}

void GfxDevice::destroyWindow() { THROW("Write me"); }

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

double GfxDevice::targetFrameTime() { return 1.0 / 60.0; }

void GfxDevice::tick() {
	SDL_GL_SwapBuffers();
	double time_diff = getTime() - m_last_time;

	bool needs_sleep = true;

	if(needs_sleep) {
		double target_diff = targetFrameTime();
		double target_time = m_last_time + target_diff;

#ifdef _WIN32
		// TODO: check if this is enough
		double busy_sleep = 0.001;
#else
		double busy_sleep = 0.0002;
#endif

		if(time_diff < target_diff) {
			sleep(target_diff - time_diff - busy_sleep);
			while(getTime() < target_time)
				;
		}
	}

	m_last_time = getTime();
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

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_KEYDOWN: {
			int key_id = translateFromSDL(event.key.keysym.sym);
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
			int button_id =
				event.button.type == SDL_BUTTON_LEFT
					? InputButton::left
					: event.button.type == SDL_BUTTON_RIGHT
						  ? InputButton::right
						  : event.button.type == SDL_BUTTON_MIDDLE ? InputButton::middle : -1;
			if(button_id != -1) {
				m_input_state.mouse_buttons[button_id] = is_down ? 1 : -1;
				events.emplace_back(is_down ? InputEvent::mouse_button_down
											: InputEvent::mouse_button_up,
									InputButton::Type(button_id));
			}
			break;
		}
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
		event.init(modifiers, m_input_state.mouse_pos, m_input_state.mouse_move, m_input_state.mouse_wheel);

	return true;
}

#ifdef __EMSCRIPTEN__
void GfxDevice::emscriptenCallback() {
	GfxDevice &inst = instance();
	DASSERT(inst.m_main_loop_function);

	inst.pollEvents();
	inst.m_main_loop_function(inst);
	inst.tick();
}
#endif

void GfxDevice::runMainLoop(MainLoopFunction function) {
	m_main_loop_function = function;
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(emscriptenCallback, 0, 1);
#else
	while(pollEvents()) {
		if(!function(*this))
			break;
		tick();
	}
#endif
	m_main_loop_function = nullptr;
}

void GfxDevice::grabMouse(bool grab) {
	//	if(grab)
	//		glfwDisable(GLFW_MOUSE_CURSOR);
	//	else
	//		glfwEnable(GLFW_MOUSE_CURSOR);
}

void GfxDevice::showCursor(bool flag) {
	//	if(flag)
	//		glfwEnable(GLFW_MOUSE_CURSOR);
	//	else
	//		glfwDisable(GLFW_MOUSE_CURSOR);
}

/*
*/

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
}
