// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/sys/platform.h"
#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#endif

#include "fwk/gfx/vulkan_device.h"

#include "fwk/format.h"
#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx/vulkan_instance.h"
#include "fwk/hash_map.h"
#include "fwk/index_range.h"
#include "fwk/math/box.h"
#include "fwk/sys/input.h"
#include "fwk/sys/thread.h"
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

static VulkanDevice *s_instance = nullptr;

bool VulkanDevice::isPresent() { return !!s_instance; }

VulkanDevice &VulkanDevice::instance() {
	DASSERT(s_instance);
	return *s_instance;
}

struct VulkanDevice::InputImpl {
	InputState state;
	vector<InputEvent> events;
	SDLKeyMap key_map;
};

struct VulkanDevice::WindowImpl {
  public:
	WindowImpl(ZStr title, IRect rect, VulkanDeviceConfig config) : config(config) {
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
		if(config.multisampling) {
			FATAL("fixme");
			DASSERT(*config.multisampling >= 1 && *config.multisampling <= 64);
			//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, *config.multisampling);
		}

		int ver_major = int(config.version);
		int ver_minor = int((config.version - float(ver_major)) * 10.0);
		/*int profile = config.profile == GlProfile::compatibility ?
						  SDL_GL_CONTEXT_PROFILE_COMPATIBILITY :
					  config.profile == GlProfile::es ? SDL_GL_CONTEXT_PROFILE_ES :
														  SDL_GL_CONTEXT_PROFILE_CORE;*/

		//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ver_major);
		//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ver_minor);
		//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);

		window = SDL_CreateWindow(title.c_str(), pos.x, pos.y, size.x, size.y, sdl_flags);
		if(!window)
			reportSDLError("SDL_CreateWindow");

		auto *vinstance = VulkanInstance::instance();
		ASSERT("VulkanInstance must be created before creating VulkanDevice" && vinstance);
		instance = vinstance->handle();

		auto phys_device = vinstance->preferredPhysicalDevice();
		ASSERT("Cannot find suitable Vulkan device" && phys_device);
		auto queues = vinstance->deviceQueueFamilies(*phys_device);

		Maybe<int> best_qf;
		for(int i : intRange(queues)) {
			auto &queue = queues[i];

			uint required_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
			if((queue.queueFlags & required_flags) != required_flags)
				continue;
			if(!best_qf || queue.queueFlags > queues[*best_qf].queueFlags)
				best_qf = i;
		}
		ASSERT("Cannot find suitable queue family for selected Vulkan device" && best_qf);

		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueCount = 1;
		queue_create_info.queueFamilyIndex = *best_qf;
		float queue_priority = 1.0;
		queue_create_info.pQueuePriorities = &queue_priority;

		vector<const char *> exts;
		exts.emplace_back("VK_KHR_swapchain");

		VkPhysicalDeviceFeatures device_features{};
		VkDeviceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.pQueueCreateInfos = &queue_create_info;
		create_info.queueCreateInfoCount = 1;
		create_info.pEnabledFeatures = &device_features;
		create_info.enabledExtensionCount = exts.size();
		create_info.ppEnabledExtensionNames = exts.data();
		// TODO: more options here

		print("Extensions: %\n", vinstance->deviceExtensions(*phys_device));

		if(vkCreateDevice(*phys_device, &create_info, nullptr, &device) != VK_SUCCESS)
			FATAL("Error when creating VkDevice...");
		vkGetDeviceQueue(device, *best_qf, 0, &queue);

		if(!SDL_Vulkan_CreateSurface(window, instance, &surface))
			reportSDLError("SDL_Vulkan_CreateSurface");

		// TODO: instead make sure that we have all the needed queues
		VkBool32 surface_supported;
		vkGetPhysicalDeviceSurfaceSupportKHR(*phys_device, *best_qf, surface, &surface_supported);
		ASSERT("Selected queue cannot present to created surface" && surface_supported);

		auto swap_chain_info = vinstance->swapChainInfo(*phys_device, surface);
		ASSERT(!swap_chain_info.formats.empty());
		ASSERT(!swap_chain_info.present_modes.empty());

		VkSwapchainCreateInfoKHR swap_create_info{};
		swap_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_create_info.surface = surface;
		swap_create_info.minImageCount = 2;
		swap_create_info.imageFormat = swap_chain_info.formats[0].format;
		swap_create_info.imageColorSpace = swap_chain_info.formats[0].colorSpace;
		swap_create_info.imageExtent = swap_chain_info.caps.currentExtent;
		swap_create_info.imageArrayLayers = 1;
		swap_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swap_create_info.preTransform = swap_chain_info.caps.currentTransform;
		swap_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swap_create_info.clipped = VK_TRUE;
		swap_create_info.presentMode = swap_chain_info.present_modes[0];

		// TODO: separate class for swap chain ?
		if(vkCreateSwapchainKHR(device, &swap_create_info, nullptr, &swap_chain) != VK_SUCCESS)
			FATAL("Error when creating swap chain");
	}

	~WindowImpl() {
		if(swap_chain)
			vkDestroySwapchainKHR(device, swap_chain, nullptr);
		if(surface)
			vkDestroySurfaceKHR(instance, surface, nullptr);
		if(device)
			vkDestroyDevice(device, nullptr);
		program_cache.clear();
		SDL_DestroyWindow(window);
	}

	SDL_Window *window;
	SDL_GLContext gl_context;
	HashMap<string, PProgram> program_cache;
	VulkanDeviceConfig config;
	VkInstance instance = 0;
	VkDevice device = 0;
	VkQueue queue = 0;
	VkSurfaceKHR surface = 0;
	VkSwapchainKHR swap_chain = 0;
};

VulkanDevice::VulkanDevice() {
	m_input_impl.emplace();
	ASSERT("Only one instance of VulkanDevice can be created at a time" && !s_instance);
	s_instance = this;

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

VulkanDevice::~VulkanDevice() {
	PASSERT_GL_THREAD();
	m_window_impl.reset();
	s_instance = nullptr;
	SDL_Quit();
}

vector<IRect> VulkanDevice::displayRects() const {
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

vector<float> VulkanDevice::displayDpiScales() const {
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

IRect VulkanDevice::sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
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

void VulkanDevice::createWindow(ZStr title, IRect rect, Config config) {
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = {title, rect, config};

	SDL_GL_SetSwapInterval(config.flags & Flag::vsync ? -1 : 0);
}

void VulkanDevice::destroyWindow() { m_window_impl.reset(); }

void VulkanDevice::setWindowTitle(ZStr title) {
	SDL_SetWindowTitle(m_window_impl->window, title.c_str());
}

void VulkanDevice::setWindowSize(const int2 &size) {
	if(m_window_impl)
		SDL_SetWindowSize(m_window_impl->window, size.x, size.y);
}

void VulkanDevice::setWindowRect(IRect rect) {
	if(m_window_impl) {
		SDL_SetWindowSize(m_window_impl->window, rect.width(), rect.height());
		SDL_SetWindowPosition(m_window_impl->window, rect.x(), rect.y());
	}
}

IRect VulkanDevice::windowRect() const {
	int2 pos, size;
	if(m_window_impl) {
		size = windowSize();
		SDL_GetWindowPosition(m_window_impl->window, &pos.x, &pos.y);
		if(platform == Platform::linux) {
			// Workaround for SDL2 bug on linux platforms
			// (TODO: find out exactly where it happens?)
			int top, left, bottom, right;
			SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
			pos -= int2{left, top};
		}
	}
	return IRect(pos, pos + size);
}

#ifdef FWK_PLATFORM_WINDOWS
Pair<int2> windowNonClientBorders(HWND hwnd);
#endif

IRect VulkanDevice::restoredWindowRect() const {
	IRect window_rect = windowRect();

#ifdef FWK_PLATFORM_WINDOWS
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_window_impl->window, &wmInfo);
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

EnumMap<RectSide, int> VulkanDevice::windowBorder() const {
	int top = 0, bottom = 0, left = 0, right = 0;
	SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
	return {{right, top, left, bottom}};
}

void VulkanDevice::setWindowFullscreen(Flags flags) {
	DASSERT(!flags || isOneOf(flags, Flag::fullscreen, Flag::fullscreen_desktop));

	if(m_window_impl) {
		uint sdl_flags = flags == Flag::fullscreen		   ? SDL_WINDOW_FULLSCREEN :
						 flags == Flag::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP :
															   0;
		SDL_SetWindowFullscreen(m_window_impl->window, sdl_flags);
		auto mask = Flag::fullscreen | Flag::fullscreen_desktop;
		m_window_impl->config.flags = (m_window_impl->config.flags & ~mask) | (mask & flags);
	}
}

bool VulkanDevice::isWindowFullscreen() const {
	return windowFlags() & (Flag::fullscreen | Flag::fullscreen_desktop);
}

bool VulkanDevice::isWindowMaximized() const {
	auto flags = SDL_GetWindowFlags(m_window_impl->window);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

int VulkanDevice::windowDisplayIndex() const {
	return SDL_GetWindowDisplayIndex(m_window_impl->window);
}

float VulkanDevice::windowDpiScale() const {
	auto display_dpis = displayDpiScales();
	auto display_index = windowDisplayIndex();
	return display_dpis.inRange(display_index) ? display_dpis[display_index] : 1.0f;
}

auto VulkanDevice::windowFlags() const -> Flags {
	return m_window_impl ? m_window_impl->config.flags : Flags();
}

int2 VulkanDevice::windowSize() const {
	int2 out;
	if(m_window_impl)
		SDL_GetWindowSize(m_window_impl->window, &out.x, &out.y);
	return out;
}

void VulkanDevice::printDeviceInfo() {
	int max_tex_size;

	FATAL("writeme");
	/*glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);

	printf("Opengl info\n"
		   "Vendor: %s\nRenderer: %s\n"
		   "Maximum texture size: %d\n",
		   vendor, renderer, max_tex_size);*/
}

int VulkanDevice::swapInterval() { return SDL_GL_GetSwapInterval(); }
void VulkanDevice::setSwapInterval(int value) { SDL_GL_SetSwapInterval(value); }

bool VulkanDevice::pollEvents() {
	m_input_impl->events =
		m_input_impl->state.pollEvents(m_input_impl->key_map, m_window_impl->window);
	for(const auto &event : m_input_impl->events)
		if(event.type() == InputEventType::quit)
			return false;
	return true;
}

void VulkanDevice::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
	PASSERT_GL_THREAD();
	ASSERT(main_loop_func);
	m_main_loop_stack.emplace_back(main_loop_func, arg);
	while(pollEvents()) {
		double time = getTime();
		m_frame_time = m_last_time < 0.0 ? 0.0 : time - m_last_time;
		m_last_time = time;

		if(!main_loop_func(*this, arg))
			break;
		SDL_GL_SwapWindow(m_window_impl->window);
	}
	m_main_loop_stack.pop_back();
}

void VulkanDevice::grabMouse(bool grab) {
	if(m_window_impl)
		SDL_SetWindowGrab(m_window_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void VulkanDevice::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

const InputState &VulkanDevice::inputState() const { return m_input_impl->state; }

const vector<InputEvent> &VulkanDevice::inputEvents() const { return m_input_impl->events; }

string VulkanDevice::clipboardText() const {
	auto ret = SDL_GetClipboardText();
	return ret ? string(ret) : string();
}

void VulkanDevice::setClipboardText(ZStr str) { SDL_SetClipboardText(str.c_str()); }

PProgram VulkanDevice::cacheFindProgram(Str name) const {
	if(!m_window_impl)
		return {};
	auto &cache = m_window_impl->program_cache;
	auto it = cache.find(name);
	return it ? it->value : PProgram();
}

void VulkanDevice::cacheAddProgram(Str name, PProgram program) {
	DASSERT(program);
	if(m_window_impl)
		m_window_impl->program_cache[name] = program;
}
}
