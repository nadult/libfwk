/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_input.h"

namespace fwk {

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
	return m_type == key_pressed && m_key == key && period > delay && m_iteration % period == 0;
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
}
