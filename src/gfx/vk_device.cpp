// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#define SDL_MAIN_HANDLED

#include "fwk/sys/platform.h"
#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#endif

#include "fwk/gfx/vk_device.h"

#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx/vulkan.h"
#include "fwk/hash_map.h"
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

void initializeVulkan();

static VkDevice *s_instance = nullptr;

bool VkDevice::isPresent() { return !!s_instance; }

VkDevice &VkDevice::instance() {
	DASSERT(s_instance);
	return *s_instance;
}

struct VkDevice::InputImpl {
	InputState state;
	vector<InputEvent> events;
	SDLKeyMap key_map;
};

struct VkDevice::WindowImpl {
  public:
	WindowImpl(ZStr title, IRect rect, VkDeviceConfig config) : config(config) {
		int sdl_flags = SDL_WINDOW_VULKAN;
		auto flags = config.flags;
		DASSERT(!((flags & Opt::fullscreen) && (flags & Opt::fullscreen_desktop)));

		int2 pos = rect.min(), size = rect.size();
		if(flags & Opt::fullscreen)
			sdl_flags |= SDL_WINDOW_FULLSCREEN;
		if(flags & Opt::fullscreen_desktop)
			sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		if(flags & Opt::resizable)
			sdl_flags |= SDL_WINDOW_RESIZABLE;
		if(flags & Opt::allow_hidpi)
			sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
		if(flags & Opt::maximized)
			sdl_flags |= SDL_WINDOW_MAXIMIZED;
		if(flags & Opt::centered)
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

		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "fwk_application";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "fwk";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		uint ext_count = 0;
		SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr);
		vector<const char *> ext_names(ext_count);
		if(!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, ext_names.data()))
			reportSDLError("SDL_Vulkan_GetInstanceExtensions");
		create_info.enabledExtensionCount = ext_count;
		create_info.ppEnabledExtensionNames = ext_names.data();

		vector<const char *> layer_names;
		if(flags & Opt::validation)
			layer_names.emplace_back("VK_LAYER_KHRONOS_validation");
		create_info.enabledLayerCount = layer_names.size();
		create_info.ppEnabledLayerNames = layer_names.data();

		// TODO: handler for errors:
		// VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

		VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
		if(result != VK_SUCCESS)
			FATAL("Error on vkCreateInstance: 0x%x\n", uint(result));
	}

	~WindowImpl() {
		vkDestroyInstance(instance, nullptr);
		program_cache.clear();
		SDL_DestroyWindow(window);
	}

	SDL_Window *window;
	SDL_GLContext gl_context;
	HashMap<string, PProgram> program_cache;
	VkDeviceConfig config;
	VkInstance instance;
};

VkDevice::VkDevice() {
	m_input_impl.emplace();
	ASSERT("Only one instance of VkDevice can be created at a time" && !s_instance);
	s_instance = this;

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
		reportSDLError("SDL_Init");

	m_last_time = -1.0;
	m_frame_time = 0.0;
}

VkDevice::~VkDevice() {
	PASSERT_GL_THREAD();
	m_window_impl.reset();
	s_instance = nullptr;
	SDL_Quit();
}

vector<IRect> VkDevice::displayRects() const {
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

vector<float> VkDevice::displayDpiScales() const {
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

IRect VkDevice::sanitizeWindowRect(CSpan<IRect> display_rects, IRect window_rect,
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

void VkDevice::createWindow(ZStr title, IRect rect, Config config) {
	ASSERT(!m_window_impl && "Window is already created (only 1 window is supported for now)");
	m_window_impl = {title, rect, config};

	SDL_GL_SetSwapInterval(config.flags & Opt::vsync ? -1 : 0);
	initializeVulkan();
}

void VkDevice::destroyWindow() { m_window_impl.reset(); }

void VkDevice::setWindowTitle(ZStr title) {
	SDL_SetWindowTitle(m_window_impl->window, title.c_str());
}

void VkDevice::setWindowSize(const int2 &size) {
	if(m_window_impl)
		SDL_SetWindowSize(m_window_impl->window, size.x, size.y);
}

void VkDevice::setWindowRect(IRect rect) {
	if(m_window_impl) {
		SDL_SetWindowSize(m_window_impl->window, rect.width(), rect.height());
		SDL_SetWindowPosition(m_window_impl->window, rect.x(), rect.y());
	}
}

IRect VkDevice::windowRect() const {
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

IRect VkDevice::restoredWindowRect() const {
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

EnumMap<RectSide, int> VkDevice::windowBorder() const {
	int top = 0, bottom = 0, left = 0, right = 0;
	SDL_GetWindowBordersSize(m_window_impl->window, &top, &left, &bottom, &right);
	return {{right, top, left, bottom}};
}

void VkDevice::setWindowFullscreen(Flags flags) {
	DASSERT(!flags || isOneOf(flags, Opt::fullscreen, Opt::fullscreen_desktop));

	if(m_window_impl) {
		uint sdl_flags = flags == Opt::fullscreen		  ? SDL_WINDOW_FULLSCREEN :
						 flags == Opt::fullscreen_desktop ? SDL_WINDOW_FULLSCREEN_DESKTOP :
															  0;
		SDL_SetWindowFullscreen(m_window_impl->window, sdl_flags);
		auto mask = Opt::fullscreen | Opt::fullscreen_desktop;
		m_window_impl->config.flags = (m_window_impl->config.flags & ~mask) | (mask & flags);
	}
}

bool VkDevice::isWindowFullscreen() const {
	return windowFlags() & (Opt::fullscreen | Opt::fullscreen_desktop);
}

bool VkDevice::isWindowMaximized() const {
	auto flags = SDL_GetWindowFlags(m_window_impl->window);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

int VkDevice::windowDisplayIndex() const {
	return SDL_GetWindowDisplayIndex(m_window_impl->window);
}

float VkDevice::windowDpiScale() const {
	auto display_dpis = displayDpiScales();
	auto display_index = windowDisplayIndex();
	return display_dpis.inRange(display_index) ? display_dpis[display_index] : 1.0f;
}

auto VkDevice::windowFlags() const -> Flags {
	return m_window_impl ? m_window_impl->config.flags : Flags();
}

int2 VkDevice::windowSize() const {
	int2 out;
	if(m_window_impl)
		SDL_GetWindowSize(m_window_impl->window, &out.x, &out.y);
	return out;
}

void VkDevice::printDeviceInfo() {
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

int VkDevice::swapInterval() { return SDL_GL_GetSwapInterval(); }
void VkDevice::setSwapInterval(int value) { SDL_GL_SetSwapInterval(value); }

bool VkDevice::pollEvents() {
	m_input_impl->events =
		m_input_impl->state.pollEvents(m_input_impl->key_map, m_window_impl->window);
	for(const auto &event : m_input_impl->events)
		if(event.type() == InputEventType::quit)
			return false;
	return true;
}

void VkDevice::runMainLoop(MainLoopFunction main_loop_func, void *arg) {
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

void VkDevice::grabMouse(bool grab) {
	if(m_window_impl)
		SDL_SetWindowGrab(m_window_impl->window, grab ? SDL_TRUE : SDL_FALSE);
}

void VkDevice::showCursor(bool flag) { SDL_ShowCursor(flag ? 1 : 0); }

const InputState &VkDevice::inputState() const { return m_input_impl->state; }

const vector<InputEvent> &VkDevice::inputEvents() const { return m_input_impl->events; }

string VkDevice::clipboardText() const {
	auto ret = SDL_GetClipboardText();
	return ret ? string(ret) : string();
}

void VkDevice::setClipboardText(ZStr str) { SDL_SetClipboardText(str.c_str()); }

PProgram VkDevice::cacheFindProgram(Str name) const {
	if(!m_window_impl)
		return {};
	auto &cache = m_window_impl->program_cache;
	auto it = cache.find(name);
	return it ? it->value : PProgram();
}

void VkDevice::cacheAddProgram(Str name, PProgram program) {
	DASSERT(program);
	if(m_window_impl)
		m_window_impl->program_cache[name] = program;
}
}
