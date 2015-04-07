/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include <memory.h>
#include <GL/glfw.h>

extern "C" {
extern struct {
	int MousePosX, MousePosY;
	int WheelPos;
	char MouseButton[GLFW_MOUSE_BUTTON_LAST + 1];
	char Key[GLFW_KEY_LAST + 1];
	int LastChar;
	int StickyKeys;
	int StickyMouseButtons;
	int KeyRepeat;
	int MouseMoved, OldMouseX, OldMouseY;
} _glfwInput;
// TODO: make proper input handling
}

namespace fwk {
namespace {

	bool s_want_close = false;
	int GLFWCALL CloseWindowHandle() {
		s_want_close = true;
		return GL_FALSE;
	}
}

struct GfxDevice::Impl {
	Impl() : last_time(-1.0), press_delay(0.2), clock(0) {}

	void initialize() {
		last_time = -1.0;
		press_delay = 0.2;
		clock = 0;

		memset(&prev_input, 0, sizeof(prev_input));
		memset(&current_input, 0, sizeof(current_input));
		glfwGetMousePos(&current_input.MousePosX, &current_input.MousePosY);

		for(int n = 0; n < InputKey::special; n++)
			key_map[n] = n;
		memset(key_map + InputKey::special, 0,
			   (InputKey::count - InputKey::special) * sizeof(key_map[0]));

		for(int n = 0; n < InputKey::count; n++)
			time_pressed[n] = -1.0;

		key_map[InputKey::space] = GLFW_KEY_SPACE;
		key_map[InputKey::special] = GLFW_KEY_SPECIAL;
		key_map[InputKey::esc] = GLFW_KEY_ESC;
		key_map[InputKey::f1] = GLFW_KEY_F1;
		key_map[InputKey::f2] = GLFW_KEY_F2;
		key_map[InputKey::f3] = GLFW_KEY_F3;
		key_map[InputKey::f4] = GLFW_KEY_F4;
		key_map[InputKey::f5] = GLFW_KEY_F5;
		key_map[InputKey::f6] = GLFW_KEY_F6;
		key_map[InputKey::f7] = GLFW_KEY_F7;
		key_map[InputKey::f8] = GLFW_KEY_F8;
		key_map[InputKey::f9] = GLFW_KEY_F9;
		key_map[InputKey::f10] = GLFW_KEY_F10;
		key_map[InputKey::f11] = GLFW_KEY_F11;
		key_map[InputKey::f12] = GLFW_KEY_F12;
		key_map[InputKey::up] = GLFW_KEY_UP;
		key_map[InputKey::down] = GLFW_KEY_DOWN;
		key_map[InputKey::left] = GLFW_KEY_LEFT;
		key_map[InputKey::right] = GLFW_KEY_RIGHT;
		key_map[InputKey::lshift] = GLFW_KEY_LSHIFT;
		key_map[InputKey::rshift] = GLFW_KEY_RSHIFT;
		key_map[InputKey::lctrl] = GLFW_KEY_LCTRL;
		key_map[InputKey::rctrl] = GLFW_KEY_RCTRL;
		key_map[InputKey::lalt] = GLFW_KEY_LALT;
		key_map[InputKey::ralt] = GLFW_KEY_RALT;
		key_map[InputKey::tab] = GLFW_KEY_TAB;
		key_map[InputKey::enter] = GLFW_KEY_ENTER;
		key_map[InputKey::backspace] = GLFW_KEY_BACKSPACE;
		key_map[InputKey::insert] = GLFW_KEY_INSERT;
		key_map[InputKey::del] = GLFW_KEY_DEL;
		key_map[InputKey::pageup] = GLFW_KEY_PAGEUP;
		key_map[InputKey::pagedown] = GLFW_KEY_PAGEDOWN;
		key_map[InputKey::home] = GLFW_KEY_HOME;
		key_map[InputKey::end] = GLFW_KEY_END;
		key_map[InputKey::kp_0] = GLFW_KEY_KP_0;
		key_map[InputKey::kp_1] = GLFW_KEY_KP_1;
		key_map[InputKey::kp_2] = GLFW_KEY_KP_2;
		key_map[InputKey::kp_3] = GLFW_KEY_KP_3;
		key_map[InputKey::kp_4] = GLFW_KEY_KP_4;
		key_map[InputKey::kp_5] = GLFW_KEY_KP_5;
		key_map[InputKey::kp_6] = GLFW_KEY_KP_6;
		key_map[InputKey::kp_7] = GLFW_KEY_KP_7;
		key_map[InputKey::kp_8] = GLFW_KEY_KP_8;
		key_map[InputKey::kp_9] = GLFW_KEY_KP_9;
		key_map[InputKey::kp_divide] = GLFW_KEY_KP_DIVIDE;
		key_map[InputKey::kp_multiply] = GLFW_KEY_KP_MULTIPLY;
		key_map[InputKey::kp_subtract] = GLFW_KEY_KP_SUBTRACT;
		key_map[InputKey::kp_add] = GLFW_KEY_KP_ADD;
		key_map[InputKey::kp_decimal] = GLFW_KEY_KP_DECIMAL;
		key_map[InputKey::kp_equal] = GLFW_KEY_KP_EQUAL;
		key_map[InputKey::kp_enter] = GLFW_KEY_KP_ENTER;
	}

	void pollEvents() {
		prev_input = current_input;
		memcpy(&current_input, &_glfwInput, sizeof(_glfwInput));
		glfwGetMousePos(&current_input.MousePosX, &current_input.MousePosY);

		double time = getTime();
		for(int n = 0; n < GLFW_KEY_LAST + 1; n++) {
			char previous = prev_input.Key[n];
			char current = current_input.Key[n];

			if(current && !previous)
				time_pressed[n] = time;
			else if(!current)
				time_pressed[n] = -1.0;
		}
		clock++;
	}

	int2 mouseMove() const {
		return int2(current_input.MousePosX - prev_input.MousePosX,
					current_input.MousePosY - prev_input.MousePosY);
	}

	bool isKeyPressed(int k) { return current_input.Key[key_map[k]]; }

	bool isKeyDown(int k) { return current_input.Key[key_map[k]] && (!prev_input.Key[key_map[k]]); }

	bool isKeyDownAuto(int k, int period) {
		int id = key_map[k];
		if(!current_input.Key[id])
			return false;
		return !prev_input.Key[id] ||
			   (last_time - time_pressed[id] > press_delay && clock % period == 0);
	}

	bool isKeyUp(int k) { return (!current_input.Key[key_map[k]]) && prev_input.Key[key_map[k]]; }

	struct InputState {
		int MousePosX, MousePosY;
		int WheelPos;
		char MouseButton[GLFW_MOUSE_BUTTON_LAST + 1];
		char Key[GLFW_KEY_LAST + 1];
		int LastChar;
		int StickyKeys;
		int StickyMouseButtons;
		int KeyRepeat;
		int MouseMoved, OldMouseX, OldMouseY;
	} prev_input, current_input;

	u32 key_map[InputKey::count];

	double time_pressed[GLFW_KEY_LAST + 1];
	double last_time;
	double press_delay;
	int clock;
};

void loadExtensions();
void initViewport(int2 size);

GfxDevice &GfxDevice::instance() {
	static GfxDevice s_instance;
	return s_instance;
}

GfxDevice::GfxDevice() : m_impl(make_unique<Impl>()), m_has_window(false) {
	if(!glfwInit())
		THROW("Error while initializing GLFW");
	glfwDisable(GLFW_AUTO_POLL_EVENTS);
}

GfxDevice::~GfxDevice() {
	if(m_has_window)
		destroyWindow();
	glfwTerminate();
}

void GfxDevice::createWindow(int2 size, bool full) {
	DASSERT(!m_has_window);
	glfwOpenWindowHint(GLFW_WINDOW_NO_RESIZE, GL_TRUE);
	if(!glfwOpenWindow(size.x, size.y, 8, 8, 8, 8, 24, 8, full ? GLFW_FULLSCREEN : GLFW_WINDOW))
		THROW("Error while initializing window with glfwOpenGLWindow");

	glfwSwapInterval(1);
	glfwSetWindowPos(0, 24);

	s_want_close = false;
	glfwSetWindowCloseCallback(CloseWindowHandle);

	m_impl->initialize();
	loadExtensions();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(0);

	initViewport(size);
	// TODO: initial mouse pos (until user moves it) is always 0,0

	glfwPollEvents();
	setBlendingMode(bmNormal);

	m_has_window = true;
}

void GfxDevice::destroyWindow() {
	DASSERT(m_has_window);
	glfwCloseWindow();
	m_has_window = false;
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

double GfxDevice::targetFrameTime() { return 1.0 / 60.0; }

void GfxDevice::tick() {
	double time_diff = getTime() - m_impl->last_time;

	if(m_has_window) {
		glfwSwapBuffers();
	} else {
		double target_diff = targetFrameTime();
		double target_time = m_impl->last_time + target_diff;

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

	m_impl->last_time = getTime();
}

bool GfxDevice::pollEvents() {
	if(m_has_window)
		glfwPollEvents();

	m_impl->pollEvents();
	return !s_want_close;
}

const int2 GfxDevice::getWindowSize() {
	DASSERT(m_has_window);

	int2 out;
	glfwGetWindowSize(&out.x, &out.y);
	return out;
}

int2 GfxDevice::getMaxWindowSize(bool is_fullscreen) {
	GLFWvidmode desktop_mode;
	glfwGetDesktopMode(&desktop_mode);

	int2 out(desktop_mode.Width, desktop_mode.Height);
	if(!is_fullscreen)
		out -= int2(4, 50); // TODO: compute these values properly
	return out;
}

void GfxDevice::adjustWindowSize(int2 &size, bool is_fullscreen) {
	int2 max_size = getMaxWindowSize(is_fullscreen);
	if(size.x == 0 || size.y == 0)
		size = max_size;
	size = min(size, max_size);
}

void GfxDevice::setWindowPos(const int2 &pos) {
	DASSERT(m_has_window);

	glfwSetWindowPos(pos.x, pos.y);
}

void GfxDevice::setWindowTitle(const char *title) {
	DASSERT(m_has_window);

	glfwSetWindowTitle(title);
	glfwPollEvents(); // TODO: remove, set window on creation
}

void GfxDevice::grabMouse(bool grab) {
	if(grab)
		glfwDisable(GLFW_MOUSE_CURSOR);
	else
		glfwEnable(GLFW_MOUSE_CURSOR);
}

void GfxDevice::showCursor(bool flag) {
	if(flag)
		glfwEnable(GLFW_MOUSE_CURSOR);
	else
		glfwDisable(GLFW_MOUSE_CURSOR);
}

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
}

bool GfxDevice::isKeyPressed(int k) { return m_impl->isKeyPressed(k); }

bool GfxDevice::isKeyDown(int k) { return m_impl->isKeyDown(k); }

bool GfxDevice::isKeyDownAuto(int k, int period) { return m_impl->isKeyDownAuto(k, period); }

bool GfxDevice::isKeyUp(int k) { return m_impl->isKeyUp(k); }

bool GfxDevice::isMouseKeyPressed(int k) { return m_impl->current_input.MouseButton[k]; }
bool GfxDevice::isMouseKeyDown(int k) {
	return m_impl->current_input.MouseButton[k] && (!m_impl->prev_input.MouseButton[k]);
}
bool GfxDevice::isMouseKeyUp(int k) {
	return (!m_impl->current_input.MouseButton[k]) && m_impl->prev_input.MouseButton[k];
}

int2 GfxDevice::getMousePos() {
	return int2(m_impl->current_input.MousePosX, m_impl->current_input.MousePosY);
}

int2 GfxDevice::getMouseMove() { return m_impl->mouseMove(); }

int GfxDevice::getMouseWheelPos() { return m_impl->current_input.WheelPos; }

int GfxDevice::getMouseWheelMove() {
	return m_impl->current_input.WheelPos - m_impl->prev_input.WheelPos;
}

vector<InputEvent> GfxDevice::generateInputEvents() {
	vector<InputEvent> out;

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

	int modifiers = (isKeyPressed(InputKey::lshift) ? InputEvent::mod_lshift : 0) |
					(isKeyPressed(InputKey::rshift) ? InputEvent::mod_rshift : 0) |
					(isKeyPressed(InputKey::lctrl) ? InputEvent::mod_lctrl : 0) |
					(isKeyPressed(InputKey::lalt) ? InputEvent::mod_lalt : 0);
	for(int n = 0; n < (int)out.size(); n++)
		out[n].init(modifiers, mouse_pos);

	return out;
}
}
