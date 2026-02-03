// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/sys/platform.h"
#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#endif

#include "fwk/vulkan/vulkan_window.h"

#include "fwk/format.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/math/box.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/input.h"
#include "fwk/sys/thread.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_internal.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <atomic>
#include <memory.h>

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace fwk {

static std::atomic<unsigned> sdl_init_counter;

static void initSdlVideo() {
	if(sdl_init_counter.fetch_add(1) == 0)
		if(!SDL_InitSubSystem(SDL_INIT_VIDEO))
			FWK_FATAL("SDL_InitSubSystem failed");
}

static void quitSdlVideo() {
	if(sdl_init_counter.fetch_sub(1) == 1)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

struct VulkanWindow::Impl {
	VWindowId id;
	VInstanceRef instance_ref;
	VWindowFlags flags = none;
	SDL_Window *window = nullptr;
	SDLKeyMap key_map;
	InputState input_state;
	VkSurfaceKHR surface_handle = 0;
	vector<InputEvent> input_events;

	double prev_fps_time = -1.0;
	double fps = 0;
	int num_frames = 0;

	double last_frame_time = -1.0;
	double frame_diff_time = -1.0;

	bool is_closing = false;
};

VWindowRef VulkanWindow::ref() { return VWindowRef(m_impl->id, this); }
VWindowId VulkanWindow::id() const { return m_impl->id; }

VulkanWindow::VulkanWindow(VWindowId id, VInstanceRef instance_ref) {
	m_impl.emplace(id, instance_ref);
}
VulkanWindow::~VulkanWindow() {
	if(m_impl->surface_handle)
		vkDestroySurfaceKHR(m_impl->instance_ref->handle(), m_impl->surface_handle, nullptr);
	if(m_impl->window)
		SDL_DestroyWindow(m_impl->window);
	quitSdlVideo();
}

Ex<VWindowRef> VulkanWindow::create(VInstanceRef instance_ref, ZStr title, IRect rect,
									VWindowFlags flags) {
	auto window = EX_PASS(g_vk_storage.allocWindow(instance_ref));
	EXPECT(window->initialize(title, rect, flags));
	return window;
}

static const EnumMap<VWindowFlag, uint> window_flags_map = {
	{SDL_WINDOW_FULLSCREEN, SDL_WINDOW_RESIZABLE, 0, SDL_WINDOW_MAXIMIZED, SDL_WINDOW_MINIMIZED,
	 SDL_WINDOW_HIGH_PIXEL_DENSITY, 0}};

Ex<void> VulkanWindow::initialize(ZStr title, IRect rect, VWindowFlags flags) {
	int sdl_flags = SDL_WINDOW_VULKAN;
	for(auto flag : flags)
		sdl_flags |= window_flags_map[flag];
	DASSERT(!((flags & Flag::minimized) && (flags & Flag::maximized)));

	initSdlVideo();
	m_impl->flags = flags;

	int2 pos = rect.min(), size = rect.size();
	if(flags & Flag::centered)
		pos.x = pos.y = SDL_WINDOWPOS_CENTERED;
	else if(flags & Flag::maximized)
		pos.x = pos.y = SDL_WINDOWPOS_UNDEFINED;
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, pos.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, pos.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, size.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, size.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, sdl_flags);
	m_impl->window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	if(!m_impl->window)
		return FWK_ERROR("SDL_CreateWindow failed: %", SDL_GetError());
	if(!SDL_Vulkan_CreateSurface(m_impl->window, m_impl->instance_ref->handle(), nullptr,
								 &m_impl->surface_handle))
		return FWK_ERROR("SDL_Vulkan_CreateSurface failed: %", SDL_GetError());

	SDL_StartTextInput(m_impl->window);

	return {};
}

VkSurfaceKHR VulkanWindow::surfaceHandle() const { return m_impl->surface_handle; }

vector<DisplayInfo> VulkanWindow::displays() {
	initSdlVideo();
	int count = 0;
	auto *display_ids = SDL_GetDisplays(&count);
	vector<DisplayInfo> out;
	out.reserve(count);
	for(int idx = 0; idx < count; idx++) {
		auto id = display_ids[idx];
		float dpi_scale = SDL_GetDisplayContentScale(id);
		if(dpi_scale == 0.0f)
			continue;
		SDL_Rect sdl_rect;
		if(!SDL_GetDisplayBounds(id, &sdl_rect))
			continue;
		const char *name = SDL_GetDisplayName(id);
		if(!name)
			continue;
		IRect rect{sdl_rect.x, sdl_rect.y, sdl_rect.x + sdl_rect.w, sdl_rect.y + sdl_rect.h};
		out.emplace_back(string(name), rect, id, dpi_scale);
	}
	quitSdlVideo();
	return out;
}

IRect VulkanWindow::sanitizeWindowRect(CSpan<DisplayInfo> displays, IRect window_rect,
									   float minimum_overlap) {
	if(!displays)
		return window_rect;
	IRect nonempty_rect{window_rect.min(), window_rect.min() + max(int2(1), window_rect.size())};

	float area = window_rect.surfaceArea(), overlap = 0.0;
	for(auto &display : displays) {
		auto isect = nonempty_rect.intersection(display.rect);
		if(isect)
			overlap += isect->surfaceArea();
	}

	if(overlap / area < minimum_overlap) {
		auto &first_display = displays[0];
		int2 new_window_pos = first_display.rect.center() - window_rect.size() / 2;
		return IRect(new_window_pos, new_window_pos + window_rect.size());
	}
	return window_rect;
}

void VulkanWindow::setTitle(ZStr title) { SDL_SetWindowTitle(m_impl->window, title.c_str()); }

void VulkanWindow::setSize(const int2 &size) { SDL_SetWindowSize(m_impl->window, size.x, size.y); }

void VulkanWindow::setRect(IRect rect) {
	SDL_SetWindowSize(m_impl->window, rect.width(), rect.height());
	SDL_SetWindowPosition(m_impl->window, rect.x(), rect.y());
}

IRect VulkanWindow::rect() const {
	int2 pos, size;
	size = this->size();
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
Pair<int2> windowNonClientBorders(HWND hwnd) {
	RECT window = {0, 0, 0, 0}, client = {0, 0, 0, 0};
	POINT client_to_screen = {0, 0};

	if(GetWindowRect(hwnd, &window) && GetClientRect(hwnd, &client) &&
	   ClientToScreen(hwnd, &client_to_screen)) {
		return {{client_to_screen.x - window.left, client_to_screen.y - window.top},
				{window.right - client_to_screen.x - client.right,
				 window.bottom - client_to_screen.y - client.bottom}};
	}
	return {};
}
#endif

IRect VulkanWindow::restoredRect() const {
	IRect window_rect = rect();

#ifdef FWK_PLATFORM_WINDOWS
	HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(m_impl->window),
											 SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
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

void VulkanWindow::setFullscreen(bool fullscreen) {
	SDL_SetWindowFullscreen(m_impl->window, fullscreen);
}

u32 VulkanWindow::displayIndex() const { return SDL_GetDisplayForWindow(m_impl->window); }
float VulkanWindow::dpiScale() const { return SDL_GetWindowDisplayScale(m_impl->window); }

VWindowFlags VulkanWindow::flags() const { return m_impl->flags; }

int2 VulkanWindow::size() const {
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
	auto update_flags = flag(VWindowFlag::fullscreen);
	m_impl->flags &= ~update_flags;
	VWindowFlags out;
	for(auto flag : update_flags)
		if(sdl_flags & window_flags_map[flag])
			out |= flag;

	return !m_impl->is_closing;
}

void VulkanWindow::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	ASSERT(main_loop_func);
	m_main_loop_stack.emplace_back(main_loop_func, arg);

	while(pollEvents()) {
		bool is_minimized = isMinimized();
		if(m_impl->flags & Flag::sleep_when_minimized && is_minimized) {
			sleep(0.01);
			continue;
		}

		updateFPS(is_minimized);
		if(!main_loop_func(*this, arg))
			break;
	}
	m_main_loop_stack.pop_back();
}

void VulkanWindow::updateFPS(bool reset) {
	if(reset) {
		m_impl->prev_fps_time = -1.0;
		m_impl->last_frame_time = -1.0;
		m_impl->frame_diff_time = -1.0;
		m_impl->num_frames = 0;
		m_impl->fps = 0.0;
		return;
	}

	double time = getTime();
	if(m_impl->prev_fps_time < 0.0)
		m_impl->prev_fps_time = time;
	else {
		m_impl->num_frames++;
		auto diff = time - m_impl->prev_fps_time;
		if(diff > 1.0) {
			m_impl->fps = double(m_impl->num_frames) / diff;
			m_impl->num_frames = 0;
			m_impl->prev_fps_time = time;
		}
	}

	if(m_impl->last_frame_time >= 0.0)
		m_impl->frame_diff_time = time - m_impl->last_frame_time;
	m_impl->last_frame_time = time;
}

Maybe<double> VulkanWindow::fps() const {
	return m_impl->fps == 0.0 ? Maybe<double>() : m_impl->fps;
}

Maybe<double> VulkanWindow::frameTimeDiff() const {
	return m_impl->frame_diff_time < 0.0 ? Maybe<double>() : m_impl->frame_diff_time;
}

void VulkanWindow::grabMouse(bool grab) { SDL_SetWindowMouseGrab(m_impl->window, grab); }

void VulkanWindow::showCursor(bool flag) {
	if(flag)
		SDL_ShowCursor();
	else
		SDL_HideCursor();
}

string VulkanWindow::clipboardText() const {
	auto ret = SDL_GetClipboardText();
	return ret ? string(ret) : string();
}

void VulkanWindow::setClipboardText(ZStr str) { SDL_SetClipboardText(str.c_str()); }

const InputState &VulkanWindow::inputState() const { return m_impl->input_state; }
const vector<InputEvent> &VulkanWindow::inputEvents() const { return m_impl->input_events; }
}
