// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/enum_map.h"
#include "fwk/math_base.h"
#include "fwk/vector.h"

namespace fwk {

class SDLKeyMap {
  public:
	SDLKeyMap();
	~SDLKeyMap();

	int to(int) const;
	int from(int) const;

  private:
	vector<Pair<int>> m_fwk_to_sdl;
	vector<Pair<int>> m_sdl_to_fwk;
};

namespace InputKey {
	enum Type {
		// For the rest of the keys use ascii characters
		space = ' ',
		special = 256,

		esc = special,
		f1,
		f2,
		f3,
		f4,
		f5,
		f6,
		f7,
		f8,
		f9,
		f10,
		f11,
		f12,
		up,
		down,
		left,
		right,
		lshift,
		rshift,
		lctrl,
		rctrl,
		lalt,
		ralt,
		tab,
		enter,
		backspace,
		insert,
		del,
		pageup,
		pagedown,
		home,
		end,

		kp_0,
		kp_1,
		kp_2,
		kp_3,
		kp_4,
		kp_5,
		kp_6,
		kp_7,
		kp_8,
		kp_9,
		kp_divide,
		kp_multiply,
		kp_subtract,
		kp_add,
		//	kp_decimal,
		kp_period,
		kp_enter,

		count
	};
}

DEFINE_ENUM(InputButton, left, right, middle);
DEFINE_ENUM(InputModifier, lshift, rshift, lctrl, rctrl, lalt, ralt);

// empty: does nothing
// mouse_over: dummy event, generated only to conveniently handle mouse input
DEFINE_ENUM(InputEventType, empty, quit, key_down, key_up, key_pressed, key_char, mouse_button_down,
			mouse_button_up, mouse_button_pressed, mouse_over);

using InputModifiers = EnumFlags<InputModifier>;

// Keeps all the information about single input event (mouse or keyboard)
// Can be empty, might contain redundant information for ease of use.
// Naming conventions:
// keys: keyboard keys
// buttons: mouse buttons
//
// down:    will be generated initially when given key/button is pressed
// up:      will be generated once when given key/button was released
// pressed: will be generated as long as given key/button is being continually pressed
// All of these events are mutually exclusive.
class InputEvent {
  public:
	using Type = InputEventType;

	InputEvent() : InputEvent(Type::empty) {}
	InputEvent(Type type);
	InputEvent(Type key_type, int key, int iter);
	InputEvent(Type mouse_type, InputButton button);
	InputEvent(char32_t);

	void init(InputModifiers, const int2 &mouse_pos, const int2 &mouse_move, int mouse_wheel);
	void offset(const int2 &offset) { m_mouse_pos += offset; }

	explicit operator bool() const { return m_type != Type::empty; }
	Type type() const { return m_type; }

	bool isMouseEvent() const {
		return m_type >= Type::mouse_button_down && m_type <= Type::mouse_over;
	}
	bool isKeyEvent() const { return m_type >= Type::key_down && m_type <= Type::key_char; }
	bool isMouseOverEvent() const { return m_type == Type::mouse_over; }

	int key() const;
	bool keyDown(int key) const;
	bool keyUp(int key) const;
	bool keyPressed(int key) const;
	bool keyDownAuto(int key, int period = 1, int delay = 12) const;

	char32_t keyChar() const { return m_char; }

	bool mouseButtonDown(InputButton) const;
	bool mouseButtonUp(InputButton) const;
	bool mouseButtonPressed(InputButton) const;

	const int2 &mousePos() const { return m_mouse_pos; }
	const int2 &mouseMove() const { return m_mouse_move; }
	int mouseWheel() const { return (int)m_mouse_wheel; }

	InputModifiers mods() const { return m_modifiers; }
	bool pressed(InputModifiers mod) const { return (m_modifiers & mod) == mod; }

	void operator>>(TextFormatter &) const;

  private:
	char32_t m_char;
	int2 m_mouse_pos, m_mouse_move;
	int m_mouse_wheel;
	int m_key, m_iteration;
	InputModifiers m_modifiers;
	Type m_type;
};

class InputState {
  public:
	using Type = InputEventType;
	InputState();

	bool isKeyDown(int key) const;
	bool isKeyUp(int key) const;
	bool isKeyPressed(int key) const;
	bool isKeyDownAuto(int key, int period = 1, int delay = 12) const;
	const string32 &text() const { return m_text; }

	bool isMouseButtonDown(InputButton) const;
	bool isMouseButtonUp(InputButton) const;
	bool isMouseButtonPressed(InputButton) const;

	const int2 &mousePos() const { return m_mouse_pos; }
	const int2 &mouseMove() const { return m_mouse_move; }
	int mouseWheelMove() const { return m_mouse_wheel; }

  private:
	vector<InputEvent> pollEvents(const SDLKeyMap &, void *);
	friend class GlDevice;
	friend class VkDevice;

	vector<Pair<int>> m_keys;
	string32 m_text;

	int2 m_mouse_pos, m_mouse_move;
	int m_mouse_wheel;
	EnumMap<InputButton, int> m_mouse_buttons;
	bool m_is_initialized;
};
}
