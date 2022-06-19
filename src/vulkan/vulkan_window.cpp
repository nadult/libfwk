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
	Config config;
	SDL_Window *window = nullptr;
	SDLKeyMap key_map;
	InputState input_state;
	VkSurfaceKHR surface_handle = 0;
	vector<InputEvent> input_events;
	double last_time = -1.0;
	double frame_time = 0.0;
};

VulkanWindow::VulkanWindow() { m_impl.emplace(); }
VulkanWindow::~VulkanWindow() {
	auto &vulkan = VulkanInstance::instance();
	if(m_swap_chain) {
		auto device_handle = vulkan[m_swap_chain->device_id].handle;
		for(auto view : m_swap_chain->image_views)
			vkDestroyImageView(device_handle, view, nullptr);
		vkDestroySwapchainKHR(device_handle, m_swap_chain->handle, nullptr);
	}
	if(m_impl) {
		if(m_impl->surface_handle)
			vkDestroySurfaceKHR(vulkan.handle(), m_impl->surface_handle, nullptr);
		if(m_impl->window)
			SDL_DestroyWindow(m_impl->window);
	}
}

FWK_MOVE_ASSIGN_RECONSTRUCT(VulkanWindow);

VulkanWindow::VulkanWindow(VulkanWindow &&rhs)
	: m_impl(move(rhs.m_impl)), m_swap_chain(move(rhs.m_swap_chain)) {}

Ex<void> VulkanWindow::exConstruct(ZStr title, IRect rect, Config config) {
	m_impl.emplace();
	m_impl->config = config;
	int sdl_flags = SDL_WINDOW_VULKAN;
	auto flags = config.flags;
	DASSERT(!((flags & Flag::fullscreen) && (flags & Flag::fullscreen_desktop)));

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
	/*if(config.multisampling) {
		FATAL("fixme");
		DASSERT(*config.multisampling >= 1 && *config.multisampling <= 64);
		//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, *config.multisampling);
	}*/

	m_impl->config = config;
	m_impl->window = SDL_CreateWindow(title.c_str(), pos.x, pos.y, size.x, size.y, sdl_flags);
	if(!m_impl->window)
		return ERROR("SDL_CreateWindow failed: %", SDL_GetError());

	ASSERT("VulkanInstance must be created before creating VulkanWindow" &&
		   VulkanInstance::isPresent());
	auto &vulkan = VulkanInstance::instance();
	VkSurfaceKHR handle;
	if(!SDL_Vulkan_CreateSurface(m_impl->window, vulkan.handle(), &handle))
		return ERROR("SDL_Vulkan_CreateSurface failed: %", SDL_GetError());
	m_impl->surface_handle = handle;

	return {};
}

Ex<void> VulkanWindow::createSwapChain(VDeviceId device_id) {
	ASSERT(VulkanInstance::isPresent());
	EXPECT(m_impl->surface_handle);

	auto &vulkan = VulkanInstance::instance();
	auto &device_info = vulkan[device_id];
	auto surf_dev_info = surfaceDeviceInfo(device_info.physical_device_id);
	EXPECT(surf_dev_info.formats && surf_dev_info.present_modes);

	VkSwapchainKHR handle;
	VkSwapchainCreateInfoKHR ci{};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = m_impl->surface_handle;
	ci.minImageCount = 2;
	ci.imageFormat = surf_dev_info.formats[0].format;
	ci.imageColorSpace = surf_dev_info.formats[0].colorSpace;
	ci.imageExtent = surf_dev_info.capabilities.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.preTransform = surf_dev_info.capabilities.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.clipped = VK_TRUE;
	ci.presentMode = surf_dev_info.present_modes[0];

	// TODO: separate class for swap chain ?
	if(vkCreateSwapchainKHR(device_info.handle, &ci, nullptr, &handle) != VK_SUCCESS)
		FATAL("Error when creating swap chain");

	VulkanSwapChainInfo info{handle, device_id};

	uint num_images = 0;
	vkGetSwapchainImagesKHR(device_info.handle, info.handle, &num_images, nullptr);
	info.images.resize(num_images);
	vkGetSwapchainImagesKHR(device_info.handle, info.handle, &num_images, info.images.data());
	info.format = ci.imageFormat;
	info.extent = ci.imageExtent;

	info.image_views.reserve(num_images);
	for(int i : intRange(info.images)) {
		VkImageViewCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.image = info.images[i];
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = info.format;
		ci.components.a = ci.components.b = ci.components.g = ci.components.r =
			VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							   .baseMipLevel = 0,
							   .levelCount = 1,
							   .baseArrayLayer = 0,
							   .layerCount = 1};
		VkImageView view;
		vkCreateImageView(device_info.handle, &ci, nullptr, &view);
		info.image_views.emplace_back(view);
	}

	m_swap_chain = move(info);
	return {};
}

VkSurfaceKHR VulkanWindow::surfaceHandle() const { return m_impl->surface_handle; }

VulkanSurfaceDeviceInfo VulkanWindow::surfaceDeviceInfo(VPhysicalDeviceId device_id) const {
	auto &vulkan = VulkanInstance::instance();
	auto phys_device = vulkan[device_id].handle;
	auto surface = m_impl->surface_handle;

	VulkanSurfaceDeviceInfo out;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &out.capabilities);
	uint count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &count, nullptr);
	out.formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &count, out.formats.data());

	count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &count, nullptr);
	out.present_modes.resize(count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &count,
											  out.present_modes.data());
	return out;
}

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

void VulkanWindow::setSize(const int2 &size) {
	if(m_impl)
		SDL_SetWindowSize(m_impl->window, size.x, size.y);
}

void VulkanWindow::setRect(IRect rect) {
	if(m_impl) {
		SDL_SetWindowSize(m_impl->window, rect.width(), rect.height());
		SDL_SetWindowPosition(m_impl->window, rect.x(), rect.y());
	}
}

IRect VulkanWindow::rect() const {
	int2 pos, size;
	if(m_impl) {
		size = this->size();
		SDL_GetWindowPosition(m_impl->window, &pos.x, &pos.y);
		if(platform == Platform::linux) {
			// Workaround for SDL2 bug on linux platforms
			// (TODO: find out exactly where it happens?)
			int top, left, bottom, right;
			SDL_GetWindowBordersSize(m_impl->window, &top, &left, &bottom, &right);
			pos -= int2{left, top};
		}
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

void VulkanWindow::setFullscreen(Flags flags) {
	DASSERT(!flags || isOneOf(flags, Flag::fullscreen, Flag::fullscreen_desktop));

	if(m_impl) {
		uint sdl_flags = flags == Flag::fullscreen		   ? SDL_WINDOW_FULLSCREEN :
						 flags == Flag::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP :
															   0;
		SDL_SetWindowFullscreen(m_impl->window, sdl_flags);
		auto mask = Flag::fullscreen | Flag::fullscreen_desktop;
		m_impl->config.flags = (m_impl->config.flags & ~mask) | (mask & flags);
	}
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

auto VulkanWindow::flags() const -> Flags { return m_impl ? m_impl->config.flags : Flags(); }

int2 VulkanWindow::size() const {
	int2 out;
	if(m_impl)
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
		SDL_GL_SwapWindow(m_impl->window);
	}
	m_main_loop_stack.pop_back();
}

void VulkanWindow::grabMouse(bool grab) {
	if(m_impl)
		SDL_SetWindowGrab(m_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void VulkanWindow::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

double VulkanWindow::frameTime() const { return m_impl->frame_time; }
const InputState &VulkanWindow::inputState() const { return m_impl->input_state; }
const vector<InputEvent> &VulkanWindow::inputEvents() const { return m_impl->input_events; }
}
