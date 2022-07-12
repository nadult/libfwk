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

using Flag = VWindowFlag;

struct VulkanWindow::Impl {
	VWindowId id;
	VInstanceRef instance_ref;
	VWindowFlags flags;
	bool sdl_initialized = false;
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

Ex<void> VulkanWindow::initialize(ZStr title, IRect rect, VWindowFlags flags) {
	m_impl->flags = flags;
	int sdl_flags = SDL_WINDOW_VULKAN;
	DASSERT(!((flags & Flag::fullscreen) && (flags & Flag::fullscreen_desktop)));

	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) // TODO: input ?
		return ERROR("SDL_InitSubSystem failed");
	m_impl->sdl_initialized = true;

	int2 pos = rect.min(), size = rect.size();
	if(flags & Flag::fullscreen)
		sdl_flags |= SDL_WINDOW_FULLSCREEN;
	if(flags & Flag::fullscreen_desktop)
		sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	if(flags & Flag::resizable)
		sdl_flags |= SDL_WINDOW_RESIZABLE;
	if(flags & Flag::allow_hidpi)
		sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if(flags & Flag::maximized)
		sdl_flags |= SDL_WINDOW_MAXIMIZED;
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

	uint sdl_flags = flags == Flag::fullscreen		   ? SDL_WINDOW_FULLSCREEN :
					 flags == Flag::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP :
														   0;
	SDL_SetWindowFullscreen(m_impl->window, sdl_flags);
	auto mask = Flag::fullscreen | Flag::fullscreen_desktop;
	// TODO: fullscreen status can also be change externally, we should be able to track it
	m_impl->flags = (m_impl->flags & ~mask) | (mask & flags);
}

bool VulkanWindow::isFullscreen() const {
	return flags() & (Flag::fullscreen | Flag::fullscreen_desktop);
}

bool VulkanWindow::isMaximized() const {
	auto flags = SDL_GetWindowFlags(m_impl->window);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
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
	m_impl->input_events = m_impl->input_state.pollEvents(m_impl->key_map, m_impl->window);
	for(const auto &event : m_impl->input_events)
		if(event.type() == InputEventType::quit)
			return false;
	return true;
}

void VulkanWindow::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	PASSERT_GL_THREAD();
	ASSERT(main_loop_func);
	m_main_loop_stack.emplace_back(main_loop_func, arg);
	while(pollEvents()) {
		double time = getTime();
		m_impl->frame_time = m_impl->last_time < 0.0 ? 0.0 : time - m_impl->last_time;
		m_impl->last_time = time;

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
