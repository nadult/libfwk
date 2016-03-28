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
	if(key_code >= 32 && key_code <= 126)
		return key_code;
	auto it = m_key_map.find(key_code);
	DASSERT(it != m_key_map.end());
	return it->second;
}

int SDLKeyMap::from(int key_code) const {
	DASSERT(key_code >= 0);
	if(key_code >= 32 && key_code <= 126)
		return key_code;
	auto it = m_inv_map.find(key_code);
	return it == m_inv_map.end() ? -1 : it->second;
}

InputEvent::InputEvent(Type type) : m_char(0), m_type(type) {
	DASSERT(m_type == mouse_over || (!isKeyEvent() && !isMouseEvent()));
}

InputEvent::InputEvent(Type key_type, int key, int iter)
	: m_char(0), m_key(key), m_iteration(iter), m_type(key_type) {
	DASSERT(isKeyEvent());
}

InputEvent::InputEvent(Type mouse_type, InputButton button)
	: m_char(0), m_key((int)button), m_type(mouse_type) {
	DASSERT(isMouseEvent());
}

InputEvent::InputEvent(wchar_t kchar) : m_char(kchar), m_type(key_char) {}

void InputEvent::init(int flags, const int2 &mouse_pos, const int2 &mouse_move, int mouse_wheel) {
	m_mouse_pos = mouse_pos;
	m_mouse_move = mouse_move;
	m_mouse_wheel = mouse_wheel;
	m_modifiers = flags;
}

bool InputEvent::keyDown(int key) const { return m_type == key_down && m_key == key; }

bool InputEvent::keyUp(int key) const { return m_type == key_up && m_key == key; }

bool InputEvent::keyPressed(int key) const { return m_type == key_pressed && m_key == key; }

bool InputEvent::keyDownAuto(int key, int period, int delay) const {
	if(keyDown(key))
		return true;
	return m_type == key_pressed && m_key == key && m_iteration > delay &&
		   (m_iteration - delay) % period == 0;
}

bool InputEvent::mouseButtonDown(InputButton key) const {
	return m_type == mouse_button_down && m_key == (int)key;
}

bool InputEvent::mouseButtonUp(InputButton key) const {
	return m_type == mouse_button_up && m_key == (int)key;
}

bool InputEvent::mouseButtonPressed(InputButton key) const {
	return m_type == mouse_button_pressed && m_key == (int)key;
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
			return pair.second == 0 || (pair.second > delay && (pair.second - delay) % period == 0);
	return false;
}

bool InputState::isMouseButtonDown(InputButton key) const { return m_mouse_buttons[key] == 1; }
bool InputState::isMouseButtonUp(InputButton key) const { return m_mouse_buttons[key] == -1; }
bool InputState::isMouseButtonPressed(InputButton key) const { return m_mouse_buttons[key] == 2; }

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

		for(auto &state : m_mouse_buttons) {
			if(state == 1)
				state = 2;
			else if(state == -1)
				state = 0;
		}
	}
	m_mouse_move = int2(0, 0);
	m_mouse_wheel = 0;
	m_text.clear();

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
		case SDL_TEXTINPUT: {
			// TODO: Should we call SDL_StartTextInput before?
			int len = strnlen(event.text.text, arraySize(event.text.text));
			auto text = toWideString(string(event.text.text, event.text.text + len), false);
			if(!text.empty()) {
				m_text += text;
				for(auto ch : text)
					events.emplace_back(ch);
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

			Maybe<InputButton> button_id;
			switch(event.button.button) {
			case SDL_BUTTON_LEFT:
				button_id = InputButton::left;
				break;
			case SDL_BUTTON_RIGHT:
				button_id = InputButton::right;
				break;
			case SDL_BUTTON_MIDDLE:
				button_id = InputButton::middle;
				break;
			}
			if(button_id) {
				m_mouse_buttons[*button_id] = is_down ? 1 : -1;
				events.emplace_back(is_down ? InputEvent::mouse_button_down
											: InputEvent::mouse_button_up,
									*button_id);
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

	for(auto button : all<InputButton>())
		if(m_mouse_buttons[button] == 2)
			events.emplace_back(InputEvent::mouse_button_pressed, button);
	events.emplace_back(InputEvent::mouse_over);

	for(auto &event : events)
		event.init(modifiers, m_mouse_pos, m_mouse_move, m_mouse_wheel);

	return events;
}
}
