// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/sys/platform.h"
#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#endif

#include "fwk/vulkan/vulkan_window.h"

#include "fwk/format.h"
#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/math/box.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/input.h"
#include "fwk/sys/thread.h"
#include "fwk/vulkan/vulkan_instance.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <memory.h>

namespace fwk {

struct VulkanWindow::Impl {
	VWindowId id;
	VInstanceRef instance_ref;
	bool sdl_initialized = false;
	bool is_closing = false;
	VWindowFlags flags = none;
	SDL_Window *window = nullptr;
	SDLKeyMap key_map;
	InputState input_state;
	VkSurfaceKHR surface_handle = 0;
	vector<InputEvent> input_events;
	double last_time = -1.0;
	double frame_time = 0.0;
};

VulkanWindow::VulkanWindow(VWindowId id, VInstanceRef instance_ref) {
	m_impl.emplace(id, instance_ref);
}
VulkanWindow::~VulkanWindow() {
	if(m_impl->surface_handle)
		vkDestroySurfaceKHR(m_impl->instance_ref->handle(), m_impl->surface_handle, nullptr);
	if(m_impl->window)
		SDL_DestroyWindow(m_impl->window);
	if(m_impl->sdl_initialized)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

Ex<VWindowRef> VulkanWindow::create(VInstanceRef instance_ref, ZStr title, IRect rect,
									VWindowFlags flags) {
	auto window = EX_PASS(g_vk_storage.allocWindow(instance_ref));
	EXPECT(window->initialize(title, rect, flags));
	return window;
}

static const EnumMap<VWindowFlag, uint> window_flags_map = {
	{SDL_WINDOW_FULLSCREEN, SDL_WINDOW_FULLSCREEN_DESKTOP, SDL_WINDOW_RESIZABLE, 0,
	 SDL_WINDOW_MAXIMIZED, SDL_WINDOW_MINIMIZED, SDL_WINDOW_ALLOW_HIGHDPI, 0}};

Ex<void> VulkanWindow::initialize(ZStr title, IRect rect, VWindowFlags flags) {
	int sdl_flags = SDL_WINDOW_VULKAN;
	for(auto flag : flags)
		sdl_flags |= window_flags_map[flag];
	DASSERT(!((flags & Flag::fullscreen) && (flags & Flag::fullscreen_desktop)));
	DASSERT(!((flags & Flag::minimized) && (flags & Flag::maximized)));

	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) // TODO: input ?
		return ERROR("SDL_InitSubSystem failed");
	m_impl->sdl_initialized = true;
	m_impl->flags = flags;

	int2 pos = rect.min(), size = rect.size();
	if(flags & Flag::centered)
		pos.x = pos.y = SDL_WINDOWPOS_CENTERED;
	m_impl->window = SDL_CreateWindow(title.c_str(), pos.x, pos.y, size.x, size.y, sdl_flags);
	if(!m_impl->window)
		return ERROR("SDL_CreateWindow failed: %", SDL_GetError());
	if(!SDL_Vulkan_CreateSurface(m_impl->window, m_impl->instance_ref->handle(),
								 &m_impl->surface_handle))
		return ERROR("SDL_Vulkan_CreateSurface failed: %", SDL_GetError());

	return {};
}

VkSurfaceKHR VulkanWindow::surfaceHandle() const { return m_impl->surface_handle; }

vector<IRect> VulkanWindow::displayRects() const {
	int count = SDL_GetNumVideoDisplays();
	vector<IRect> out(count);
	for(int idx = 0; idx < count; idx++) {
		SDL_Rect rect;
		if(SDL_GetDisplayBounds(idx, &rect) != 0)
			FWK_FATAL("Error in SDL_GetDisplayBounds");
		out[idx] = {rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
	}
	return out;
}

vector<float> VulkanWindow::displayDpiScales() const {
	int count = SDL_GetNumVideoDisplays();
	vector<float> out(count);
	for(int idx = 0; idx < count; idx++) {
		// TODO: make sure that it's correct for all different platforms
		const float default_dpi = 96;
		float current_dpi = default_dpi;
		SDL_GetDisplayDPI(idx, nullptr, &current_dpi, nullptr);
		out[idx] = current_dpi / default_dpi;
	}
	return out;
}

IRect VulkanWindow::sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
									   float minimum_overlap) {
	if(!display_rects)
		return window_rect;
	IRect nonempty_rect{window_rect.min(), window_rect.min() + max(int2(1), window_rect.size())};

	float area = window_rect.surfaceArea(), overlap = 0.0;
	for(auto &display_rect : display_rects) {
		auto isect = nonempty_rect.intersection(display_rect);
		if(isect)
			overlap += isect->surfaceArea();
	}

	if(overlap / area < minimum_overlap) {
		auto first_display = display_rects[0];
		int2 new_window_pos = first_display.center() - window_rect.size() / 2;
		return IRect(new_window_pos, new_window_pos + window_rect.size());
	}
	return window_rect;
}

void VulkanWindow::setTitle(ZStr title) { SDL_SetWindowTitle(m_impl->window, title.c_str()); }

void VulkanWindow::setExtent(const int2 &size) {
	SDL_SetWindowSize(m_impl->window, size.x, size.y);
}

void VulkanWindow::setRect(IRect rect) {
	SDL_SetWindowSize(m_impl->window, rect.width(), rect.height());
	SDL_SetWindowPosition(m_impl->window, rect.x(), rect.y());
}

IRect VulkanWindow::rect() const {
	int2 pos, size;
	size = this->extent();
	SDL_GetWindowPosition(m_impl->window, &pos.x, &pos.y);
	if(platform == Platform::linux) {
		// Workaround for SDL2 bug on linux platforms
		// (TODO: find out exactly where it happens?)
		int top, left, bottom, right;
		SDL_GetWindowBordersSize(m_impl->window, &top, &left, &bottom, &right);
		pos -= int2{left, top};
	}
	return IRect(pos, pos + size);
}

#ifdef FWK_PLATFORM_WINDOWS
Pair<int2> windowNonClientBorders(HWND hwnd);
#endif

IRect VulkanWindow::restoredRect() const {
	IRect window_rect = rect();

#ifdef FWK_PLATFORM_WINDOWS
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_impl->window, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;
	WINDOWPLACEMENT placement;
	GetWindowPlacement(hwnd, &placement);
	IRect out(placement.rcNormalPosition.left, placement.rcNormalPosition.top,
			  placement.rcNormalPosition.right, placement.rcNormalPosition.bottom);
	auto borders = windowNonClientBorders(hwnd);
	return {out.min() + borders.first, out.max() - borders.second};
#else
	return window_rect;
#endif
}

EnumMap<RectSide, int> VulkanWindow::border() const {
	int top = 0, bottom = 0, left = 0, right = 0;
	SDL_GetWindowBordersSize(m_impl->window, &top, &left, &bottom, &right);
	return {{right, top, left, bottom}};
}

void VulkanWindow::setFullscreen(VWindowFlags flags) {
	DASSERT(!flags || isOneOf(flags, Flag::fullscreen, Flag::fullscreen_desktop));

	uint sdl_flags = 0;
	for(auto flag : flags)
		sdl_flags |= window_flags_map[flag];
	SDL_SetWindowFullscreen(m_impl->window, sdl_flags);
}

int VulkanWindow::displayIndex() const { return SDL_GetWindowDisplayIndex(m_impl->window); }

float VulkanWindow::dpiScale() const {
	auto display_dpis = displayDpiScales();
	auto display_index = displayIndex();
	return display_dpis.inRange(display_index) ? display_dpis[display_index] : 1.0f;
}

VWindowFlags VulkanWindow::flags() const { return m_impl->flags; }

int2 VulkanWindow::extent() const {
	int2 out;
	SDL_GetWindowSize(m_impl->window, &out.x, &out.y);
	return out;
}

bool VulkanWindow::pollEvents() {
	vector<WindowEvent> window_events;
	m_impl->input_events.clear();
	m_impl->input_state.pollEvents(m_impl->key_map, m_impl->input_events, window_events,
								   m_impl->window);
	for(auto event : window_events) {
		switch(event) {
		case WindowEvent::maximized:
			m_impl->flags &= ~Flag::minimized;
			m_impl->flags |= Flag::maximized;
			break;
		case WindowEvent::minimized:
			m_impl->flags |= Flag::minimized;
			m_impl->flags &= ~Flag::maximized;
			break;
		case WindowEvent::restored:
			m_impl->flags &= ~(Flag::maximized | Flag::minimized);
			break;
		case WindowEvent::quit:
			m_impl->is_closing = true;
			break;
		}
	}

	auto sdl_flags = SDL_GetWindowFlags(m_impl->window);
	auto update_flags = VWindowFlag::fullscreen | VWindowFlag::fullscreen_desktop;
	m_impl->flags &= ~update_flags;
	VWindowFlags out;
	for(auto flag : update_flags)
		if(sdl_flags & window_flags_map[flag])
			out |= flag;

	return !m_impl->is_closing;
}

void VulkanWindow::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	PASSERT_GL_THREAD();
	ASSERT(main_loop_func);
	m_main_loop_stack.emplace_back(main_loop_func, arg);
	while(pollEvents()) {
		double time = getTime();
		m_impl->frame_time = m_impl->last_time < 0.0 ? 0.0 : time - m_impl->last_time;
		m_impl->last_time = time;

		if(m_impl->flags & Flag::sleep_when_minimized && isMinimized()) {
			sleep(0.01);
			continue;
		}
		if(!main_loop_func(*this, arg))
			break;
	}
	m_main_loop_stack.pop_back();
}

void VulkanWindow::grabMouse(bool grab) {
	SDL_SetWindowGrab(m_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void VulkanWindow::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

double VulkanWindow::frameTime() const { return m_impl->frame_time; }
const InputState &VulkanWindow::inputState() const { return m_impl->input_state; }
const vector<InputEvent> &VulkanWindow::inputEvents() const { return m_impl->input_events; }
}
