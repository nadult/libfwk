/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_input.h"
#include "fwk_gfx.h"

#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <SDL.h>

namespace fwk {

SDLKeyMap::SDLKeyMap() {
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
}

SDLKeyMap::~SDLKeyMap() = default;

int SDLKeyMap::to(int key_code) const {
	DASSERT(key_code >= 0);
	if(key_code >= 0 && key_code <= 255)
		return key_code;
	auto it = m_key_map.find(key_code);
	DASSERT(it != m_key_map.end());
	return it->second;
}

int SDLKeyMap::from(int key_code) const {
	DASSERT(key_code >= 0);
	if(key_code >= 0 && key_code <= 255)
		return key_code;
	auto it = m_inv_map.find(key_code);
	return it == m_inv_map.end() ? -1 : it->second;
}

/*
char GfxDevice::getCharPressed() {
if(isKeyPressed(InputKey::space))
	return ' ';

char numerics_s[11] = ")!@#$%^&*(";
char map[][2] = {
	{'-', '_'},
	{'`', '~'},
	{'=', '+'},
	{'[', '{'},
	{']', '}'},
	{';', ':'},
	{'\'', '"'},
	{',', '<'},
	{'.', '>'},
	{'/', '?'},
	{'\\', '|'},
};

bool shift = isKeyPressed(InputKey::lshift) || isKeyPressed(InputKey::rshift); // TODO: capslock

for(int i = 0; i < (int)sizeof(map) / 2; i++)
	if(isKeyPressed(map[i][0]))
		return map[i][shift ? 1 : 0];

for(int k = 32; k < 128; k++) {
	if(!isKeyPressed(k))
		continue;
	if(k >= 'A' && k <= 'Z')
		return shift ? k : k - 'A' + 'a';
	if(k >= '0' && k <= '9')
		return shift ? numerics_s[k - '0'] : k;
}

return 0;
}*/

InputEvent::InputEvent(Type type) : m_type(type) {
	DASSERT(m_type == mouse_over || (!isKeyEvent() && !isMouseEvent()));
}

InputEvent::InputEvent(Type key_type, int key, int iter)
	: m_key(key), m_iteration(iter), m_type(key_type) {
	DASSERT(isKeyEvent());
}

InputEvent::InputEvent(Type mouse_type, InputButton::Type button)
	: m_key(button), m_type(mouse_type) {
	DASSERT(isMouseEvent());
}

void InputEvent::init(int flags, const int2 &mouse_pos, const int2 &mouse_move, int mouse_wheel) {
	m_mouse_pos = mouse_pos;
	m_mouse_move = mouse_move;
	m_mouse_wheel = mouse_wheel;
	m_modifiers = flags;
}

void InputEvent::translate(const int2 &offset) { m_mouse_pos += offset; }

bool InputEvent::keyDown(int key) const { return m_type == key_down && m_key == key; }

bool InputEvent::keyUp(int key) const { return m_type == key_up && m_key == key; }

bool InputEvent::keyPressed(int key) const { return m_type == key_pressed && m_key == key; }

bool InputEvent::keyDownAuto(int key, int period, int delay) const {
	THROW("verify me");
	return m_type == key_pressed && m_key == key && m_iteration > delay &&
		   (m_iteration - delay) % period == 0;
}

bool InputEvent::mouseButtonDown(InputButton::Type key) const {
	return m_type == mouse_button_down && m_key == key;
}

bool InputEvent::mouseButtonUp(InputButton::Type key) const {
	return m_type == mouse_button_up && m_key == key;
}

bool InputEvent::mouseButtonPressed(InputButton::Type key) const {
	return m_type == mouse_button_pressed && m_key == key;
}

InputState::InputState() : m_mouse_wheel(0), m_is_initialized(0) {
	for(auto &button : m_mouse_buttons)
		button = 0;
}

bool InputState::isKeyDown(int key) const {
	for(auto pair : m_keys)
		if(pair.first == key)
			return pair.second == 0;
	return false;
}

bool InputState::isKeyUp(int key) const {
	for(auto pair : m_keys)
		if(pair.first == key)
			return pair.second == -1;
	return false;
}

bool InputState::isKeyPressed(int key) const {
	for(auto pair : m_keys)
		if(pair.first == key)
			return pair.second > 0;
	return false;
}

bool InputState::isKeyDownAuto(int key, int period, int delay) const {
	for(auto pair : m_keys)
		if(pair.first == key)
			return pair.second > delay && (pair.second - delay) % period == 0;
	return false;
}

bool InputState::isMouseButtonDown(InputButton::Type key) const {
	DASSERT(InputButton::isValid(key));
	return m_mouse_buttons[key] == 1;
}

bool InputState::isMouseButtonUp(InputButton::Type key) const {
	DASSERT(InputButton::isValid(key));
	return m_mouse_buttons[key] == -1;
}

bool InputState::isMouseButtonPressed(InputButton::Type key) const {
	DASSERT(InputButton::isValid(key));
	return m_mouse_buttons[key] == 2;
}

vector<InputEvent> InputState::pollEvents(const SDLKeyMap &key_map) {
	vector<InputEvent> events;
	SDL_Event event;

	if(!m_is_initialized) {
		SDL_GetMouseState(&m_mouse_pos.x, &m_mouse_pos.y);
		m_is_initialized = true;
	} else {
		for(auto &key_state : m_keys)
			if(key_state.second >= 0)
				key_state.second++;

		m_keys.resize(std::remove_if(m_keys.begin(), m_keys.end(), [](const pair<int, int> &state) {
			return state.second == -1;
		}) - m_keys.begin());

		for(int n = 0; n < InputButton::count; n++) {
			auto &state = m_mouse_buttons[n];
			if(state == 1)
				state = 2;
			else if(state == -1)
				state = 0;
		}
	}
	m_mouse_move = int2(0, 0);
	m_mouse_wheel = 0;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_KEYDOWN: {
			int key_id = key_map.from(event.key.keysym.sym);
			if(key_id == -1)
				break;

			bool is_pressed = false;
			for(const auto &key_state : m_keys)
				if(key_state.first == key_id)
					is_pressed = true;
			if(!is_pressed) {
				m_keys.emplace_back(key_id, 0);
				events.emplace_back(InputEvent::key_down, key_id, 0);
			}
			break;
		}
		case SDL_KEYUP: {
			int key_id = key_map.from(event.key.keysym.sym);
			if(key_id == -1)
				break;

			events.emplace_back(InputEvent::key_up, key_id, 0);
			for(auto &key_state : m_keys)
				if(key_state.first == key_id)
					key_state.second = -1;
			break;
		}
		case SDL_MOUSEMOTION:
			m_mouse_move += int2(event.motion.xrel, event.motion.yrel);
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
				m_mouse_buttons[button_id] = is_down ? 1 : -1;
				events.emplace_back(is_down ? InputEvent::mouse_button_down
											: InputEvent::mouse_button_up,
									InputButton::Type(button_id));
			}
			break;
		}
		case SDL_MOUSEWHEEL:
			m_mouse_wheel = event.wheel.y;
			break;
		case SDL_QUIT:
			events.emplace_back(InputEvent::quit);
			break;
		}
	}

	SDL_GetMouseState(&m_mouse_pos.x, &m_mouse_pos.y);
	int modifiers = 0;

	for(const auto &key_state : m_keys) {
		if(key_state.second >= 1)
			events.emplace_back(InputEvent::key_pressed, key_state.first, key_state.second);
		if(key_state.second >= 0) {
			modifiers |= (key_state.first == InputKey::lshift ? InputEvent::mod_lshift : 0) |
						 (key_state.first == InputKey::rshift ? InputEvent::mod_rshift : 0) |
						 (key_state.first == InputKey::lctrl ? InputEvent::mod_lctrl : 0) |
						 (key_state.first == InputKey::lalt ? InputEvent::mod_lalt : 0);
		}
	}

	for(int n = 0; n < InputButton::count; n++)
		if(m_mouse_buttons[n] == 2)
			events.emplace_back(InputEvent::mouse_button_pressed, InputButton::Type(n));
	events.emplace_back(InputEvent::mouse_over);

	for(auto &event : events)
		event.init(modifiers, m_mouse_pos, m_mouse_move, m_mouse_wheel);

	return events;
}
}
