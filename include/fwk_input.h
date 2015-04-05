/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_INPUT_H
#define FWK_INPUT_H

#include "fwk.h"
#include "fwk_math.h"

namespace fwk {
namespace io {

	namespace Key {
		enum Type {
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
			kp_decimal,
			kp_equal,
			kp_enter,

			count
		};
	}

	class InputEvent {
	  public:
		enum Type {
			invalid,
			key_down,
			key_down_auto,
			key_up,
			key_pressed,

			mouse_key_down,
			mouse_key_up,
			mouse_key_pressed,
			mouse_over,
			mouse_wheel,
		};

		enum Modifier {
			mod_lshift = 1,
			mod_rshift = 2,
			mod_lctrl = 4,
			mod_lalt = 8,
		};

		InputEvent() : m_type(invalid) {}
		InputEvent(Type key_type, int key, int iter);
		InputEvent(Type mouse_type, int key, const int2 &mouse_move);
		void init(int flags, const int2 &mouse_pos);
		void translate(const float2 &offset);

		Type type() const { return m_type; }
		bool isMouseEvent() const { return m_type >= mouse_key_down && m_type <= mouse_wheel; }
		bool isKeyEvent() const { return m_type >= key_down && m_type <= key_pressed; }

		bool keyDown(int key) const;
		bool keyUp(int key) const;
		bool keyPressed(int key) const;
		bool keyDownAuto(int key, int period = 1) const;
		int key() const {
			DASSERT(m_type >= key_down && m_type <= key_pressed);
			return m_key;
		}
		int keyChar() const {
			DASSERT(m_type >= key_down && m_type <= key_pressed);
			return m_key_char;
		}

		bool mouseKeyDown(int key) const;
		bool mouseKeyUp(int key) const;
		bool mouseKeyPressed(int key) const;
		bool mouseOver() const { return m_type == mouse_over; }

		const float2 &mousePos() const {
			DASSERT(m_type != invalid);
			return m_mouse_pos;
		}
		const float2 &mouseMove() const {
			DASSERT(m_type == mouse_over);
			return m_mouse_move;
		}
		const int mouseWheel() const {
			DASSERT(m_type == mouse_wheel);
			return (int)m_mouse_move.x;
		}

		bool hasModifier(Modifier modifier) const { return m_modifiers & modifier; }

	  private:
		float2 m_mouse_pos, m_mouse_move;
		int m_key, m_key_char, m_iteration, m_modifiers;
		Type m_type;
	};
}
}

#endif
