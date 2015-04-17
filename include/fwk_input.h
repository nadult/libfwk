/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_INPUT_H
#define FWK_INPUT_H

#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

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

namespace InputButton {
	enum Type {
		left,
		right,
		middle,

		count
	};
};

class InputEvent {
  public:
	enum Type {
		invalid,
		quit,

		key_down,
		key_up,
		key_pressed,

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

	InputEvent() : m_type(invalid) {}
	InputEvent(Type type);
	InputEvent(Type key_type, int key, int iter);
	InputEvent(Type mouse_type, InputButton::Type button);
	void init(int flags, const int2 &mouse_pos, const int2 &mouse_move, int mouse_wheel);
	void translate(const int2 &offset);

	Type type() const { return m_type; }

	bool isMouseEvent() const { return m_type >= mouse_button_down && m_type <= mouse_over; }
	bool isKeyEvent() const { return m_type >= key_down && m_type <= key_pressed; }
	bool isMouseOverEvent() const { return m_type == mouse_over; }

	bool keyDown(int key) const;
	bool keyUp(int key) const;

	bool keyPressed(int key) const;
	bool keyDownAuto(int key, int period = 1, int delay = 12) const;

	int key() const {
		DASSERT(m_type != invalid);
		return isKeyEvent() ? m_key : 0;
	}
	int keyUnicode() const {
		DASSERT(m_type != invalid);
		THROW("Add support for SDL text input");
		return 0;
	}

	bool mouseKeyDown(InputButton::Type) const;
	bool mouseKeyUp(InputButton::Type) const;
	bool mouseKeyPressed(InputButton::Type) const;

	// These can be called for every valid event type
	const int2 &mousePos() const {
		DASSERT(m_type != invalid);
		return m_mouse_pos;
	}
	const int2 &mouseMove() const {
		DASSERT(m_type != invalid);
		return m_mouse_move;
	}
	int mouseWheel() const {
		DASSERT(m_type != invalid);
		THROW("Add support for mouse wheel\n");
		return (int)m_mouse_wheel;
	}

	bool hasModifier(Modifier modifier) const { return m_modifiers & modifier; }

  private:
	int2 m_mouse_pos, m_mouse_move;
	int m_mouse_wheel;
	int m_key;
	int m_iteration, m_modifiers;
	Type m_type;
};
}

#endif
