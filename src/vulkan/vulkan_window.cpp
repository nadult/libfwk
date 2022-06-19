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

static void reportSDLError(const char *func_name) {
	FATAL("Error on %s: %s", func_name, SDL_GetError());
}

struct VulkanWindow::InputImpl {
	InputState state;
	vector<InputEvent> events;
	SDLKeyMap key_map;
};

struct VulkanWindow::Impl {
	~Impl() {
		destroySwapChain();
		if(surface)
			vkDestroySurfaceKHR(instance, surface, nullptr);
		SDL_DestroyWindow(window);
	}

	void destroySwapChain() {
		for(auto view : swap_chain_image_views)
			vkDestroyImageView(device, view, nullptr);
		swap_chain_image_views.clear();
		if(swap_chain)
			vkDestroySwapchainKHR(device, swap_chain, nullptr);
		swap_chain = 0;
	}

	Config config;
	SDL_Window *window = nullptr;
	VkInstance instance = 0;
	VkDevice device = 0;
	VkQueue queue = 0;
	VkSurfaceKHR surface = 0;

	VkSwapchainKHR swap_chain = 0;
	vector<VkImage> swap_chain_images;
	vector<VkImageView> swap_chain_image_views;
	VkFormat swap_chain_format;
	VkExtent2D swap_chain_extent;
};

VulkanWindow::VulkanWindow() {
	m_input_impl.emplace();
	m_last_time = -1.0;
	m_frame_time = 0.0;
}

VulkanWindow::~VulkanWindow() = default;

FWK_MOVE_ASSIGN_RECONSTRUCT(VulkanWindow);

VulkanWindow::VulkanWindow(VulkanWindow &&rhs)
	: m_impl(move(rhs.m_impl)), m_input_impl(move(rhs.m_input_impl)), m_last_time(rhs.m_last_time),
	  m_frame_time(rhs.m_frame_time) {}

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

	m_impl->window = SDL_CreateWindow(title.c_str(), pos.x, pos.y, size.x, size.y, sdl_flags);
	if(!m_impl->window)
		reportSDLError("SDL_CreateWindow");

	ASSERT("VulkanInstance must be created before creating VulkanWindow" &&
		   VulkanInstance::isPresent());
	auto &vinstance = VulkanInstance::instance();
	m_impl->instance = vinstance.handle();

	if(!SDL_Vulkan_CreateSurface(m_impl->window, m_impl->instance, &m_impl->surface))
		return ERROR("SDL_Vulkan_CreateSurface failed: %", SDL_GetError());
	return {};
}

Ex<void> VulkanWindow::createSwapChain(VDeviceId device_id) {
	ASSERT(VulkanInstance::isPresent());
	EXPECT(m_impl && m_impl->surface);
	auto &instance = VulkanInstance::instance();
	auto &device_info = instance[device_id];
	auto &phys_info = instance[device_info.physical_device_id];
	auto swap_chain_info = phys_info.swapChainInfo(m_impl->surface);
	ASSERT(!swap_chain_info.formats.empty());
	ASSERT(!swap_chain_info.present_modes.empty());
	m_impl->device = device_info.handle;

	VkSwapchainCreateInfoKHR ci{};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = m_impl->surface;
	ci.minImageCount = 2;
	ci.imageFormat = swap_chain_info.formats[0].format;
	ci.imageColorSpace = swap_chain_info.formats[0].colorSpace;
	ci.imageExtent = swap_chain_info.caps.currentExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.preTransform = swap_chain_info.caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.clipped = VK_TRUE;
	ci.presentMode = swap_chain_info.present_modes[0];

	// TODO: separate class for swap chain ?
	if(vkCreateSwapchainKHR(device_info.handle, &ci, nullptr, &m_impl->swap_chain) != VK_SUCCESS)
		FATAL("Error when creating swap chain");

	uint num_images = 0;
	vkGetSwapchainImagesKHR(device_info.handle, m_impl->swap_chain, &num_images, nullptr);
	m_impl->swap_chain_images.resize(num_images);
	vkGetSwapchainImagesKHR(device_info.handle, m_impl->swap_chain, &num_images,
							m_impl->swap_chain_images.data());
	m_impl->swap_chain_format = ci.imageFormat;
	m_impl->swap_chain_extent = ci.imageExtent;

	m_impl->swap_chain_image_views.reserve(num_images);
	for(int i : intRange(m_impl->swap_chain_images)) {
		VkImageViewCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ci.image = m_impl->swap_chain_images[i];
		ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ci.format = m_impl->swap_chain_format;
		ci.components.a = ci.components.b = ci.components.g = ci.components.r =
			VK_COMPONENT_SWIZZLE_IDENTITY;
		ci.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							   .baseMipLevel = 0,
							   .levelCount = 1,
							   .baseArrayLayer = 0,
							   .layerCount = 1};
		VkImageView view;
		vkCreateImageView(device_info.handle, &ci, nullptr, &view);
		m_impl->swap_chain_image_views.emplace_back(view);
	}

	return {};
}

VkSurfaceKHR VulkanWindow::surfaceHandle() { return m_impl->surface; }

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
	m_input_impl->events = m_input_impl->state.pollEvents(m_input_impl->key_map, m_impl->window);
	for(const auto &event : m_input_impl->events)
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
		m_frame_time = m_last_time < 0.0 ? 0.0 : time - m_last_time;
		m_last_time = time;

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

const InputState &VulkanWindow::inputState() const { return m_input_impl->state; }
const vector<InputEvent> &VulkanWindow::inputEvents() const { return m_input_impl->events; }
}
