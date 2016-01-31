/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_INPUT_H
#define FWK_INPUT_H

#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

class SDLKeyMap {
  public:
	SDLKeyMap();
	~SDLKeyMap();
	int to(int) const;
	int from(int) const;

  private:
	std::map<int, int> m_key_map;
	std::map<int, int> m_inv_map;
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
		//		kp_decimal,
		kp_period,
		kp_enter,

		count
	};
}

DEFINE_ENUM(InputButton, left, right, middle);

class InputEvent {
  public:
	enum Type {
		quit,

		key_down,
		key_up,
		key_pressed,
		key_char,

		mouse_button_down,
		mouse_button_up,
		mouse_button_pressed,
		mouse_over, // dummy event, generated only to conveniently handle mouse input
	};

	enum Modifier {
		mod_lshift = 1,
		mod_rshift = 2,
		mod_lctrl = 4,
		mod_lalt = 8,
	};

	InputEvent(Type type);
	InputEvent(Type key_type, int key, int iter);
	InputEvent(Type mouse_type, InputButton button);
	InputEvent(wchar_t);

	void init(int flags, const int2 &mouse_pos, const int2 &mouse_move, int mouse_wheel);
	void offset(const int2 &offset) { m_mouse_pos += offset; }

	Type type() const { return m_type; }

	bool isMouseEvent() const { return m_type >= mouse_button_down && m_type <= mouse_over; }
	bool isKeyEvent() const { return m_type >= key_down && m_type <= key_pressed; }
	bool isMouseOverEvent() const { return m_type == mouse_over; }

	bool keyDown(int key) const;
	bool keyUp(int key) const;

	bool keyPressed(int key) const;
	bool keyDownAuto(int key, int period = 1, int delay = 12) const;

	int key() const { return isKeyEvent() ? m_key : 0; }
	wchar_t keyChar() const { return m_char; }

	bool mouseButtonDown(InputButton) const;
	bool mouseButtonUp(InputButton) const;
	bool mouseButtonPressed(InputButton) const;

	const int2 &mousePos() const { return m_mouse_pos; }
	const int2 &mouseMove() const { return m_mouse_move; }
	int mouseWheel() const { return (int)m_mouse_wheel; }

	bool hasModifier(Modifier modifier) const { return m_modifiers & modifier; }

  private:
	wchar_t m_char;
	int2 m_mouse_pos, m_mouse_move;
	int m_mouse_wheel;
	int m_key, m_iteration, m_modifiers;
	Type m_type;
};

class InputState {
  public:
	InputState();

	bool isKeyDown(int key) const;
	bool isKeyUp(int key) const;
	bool isKeyPressed(int key) const;
	bool isKeyDownAuto(int key, int period = 1, int delay = 12) const;
	const wstring &text() const { return m_text; }

	bool isMouseButtonDown(InputButton) const;
	bool isMouseButtonUp(InputButton) const;
	bool isMouseButtonPressed(InputButton) const;

	const int2 &mousePos() const { return m_mouse_pos; }
	const int2 &mouseMove() const { return m_mouse_move; }
	int mouseWheelMove() const { return m_mouse_wheel; }

  private:
	vector<InputEvent> pollEvents(const SDLKeyMap &);
	friend class GfxDevice;

	using KeyStatus = pair<int, int>;
	vector<KeyStatus> m_keys;
	wstring m_text;

	int2 m_mouse_pos, m_mouse_move;
	int m_mouse_wheel;
	EnumMap<int, InputButton> m_mouse_buttons;
	bool m_is_initialized;
};
}

#endif
