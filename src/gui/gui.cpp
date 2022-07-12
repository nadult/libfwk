// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gui/gui.h"

#include "fwk/any_config.h"
#include "fwk/gfx/font_finder.h"
#include "fwk/gui/imgui_internal.h"
#include "fwk/sys/input.h"
#include "gui_impl.h"

#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_instance.h"
#include "fwk/vulkan/vulkan_render_graph.h"
#include "fwk/vulkan/vulkan_swap_chain.h"
#include "fwk/vulkan/vulkan_window.h"

#include "../extern/imgui/backends/imgui_impl_vulkan.cpp"

namespace fwk {

Gui *Gui::s_instance = nullptr;

static const char *getClipboardText(void *) {
	static string buffer;
	// TODO: fixme
	//buffer = GlDevice::instance().clipboardText();
	return buffer.c_str();
}

static void setClipboardText(void *, const char *text) {
	// TODO: fixme
	//GlDevice::instance().setClipboardText(text);
}

void Gui::updateDpiAndFonts(VulkanWindow &window, bool is_initial) {
	auto dpi_scale = window.dpiScale();
	if(dpi_scale == m_dpi_scale && !is_initial)
		return;
	float scale_change = dpi_scale / m_dpi_scale;
	m_dpi_scale = dpi_scale;

	static const ImWchar glyph_ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x017F, // Latin Extended-A
		0x0180, 0x024F, // Latin Extended-B
		0x370,	0x3FF, // Greek
		0,
	};

	float font_size = m_impl->font_size * dpi_scale;
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	io.FontDefault =
		io.Fonts->AddFontFromFileTTF(m_impl->font_path.c_str(), font_size, 0, glyph_ranges);

	if(is_initial) {
		auto &device = *m_impl->device;
		auto queues = device.queues();
		auto cmd_pool =
			device.createCommandPool(queues[0].second, VCommandPoolFlag::transient).get();
		auto cmd_buffer = device.allocCommandBuffer(cmd_pool).get();

		// TODO: error handling
		VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd_buffer, &begin_info);
		ImGui_ImplVulkan_CreateFontsTexture(cmd_buffer);

		VkSubmitInfo end_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &cmd_buffer;
		vkEndCommandBuffer(cmd_buffer);
		vkQueueSubmit(queues[0].first, 1, &end_info, VK_NULL_HANDLE);

		vkDeviceWaitIdle(device.handle());
		ImGui_ImplVulkan_DestroyFontUploadObjects();
		vkDestroyCommandPool(device.handle(), cmd_pool, nullptr);
	}

	// TODO: font recreation
	/*auto *bd = ImGui_ImplVulkan_GetBackendData();
	if(bd && bd->FontImage) {
		FATAL("TODO: fixme");
	}*/

	if(!is_initial)
		rescaleWindows(scale_change);
}

void Gui::rescaleWindows(float scale) {
	auto *context = ImGui::GetCurrentContext();
	auto scale_vec = [=](ImVec2 vec) { return ImVec2(vec.x * scale, vec.y * scale); };
	for(auto &window : context->Windows) {
		window->Pos = scale_vec(window->Pos);
		window->Size = scale_vec(window->Size);
		window->SizeFull = scale_vec(window->SizeFull);
	}
}

Gui::Gui(VDeviceRef device, VWindowRef window, PVRenderPass rpass, GuiConfig opts) {
	m_impl.emplace(device);
	ASSERT("You can only create a single instance of Gui" && !s_instance);
	s_instance = this;
	ImGui::CreateContext();

	// Build texture atlas
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;

	int font_size_bonus = 0;
	if(!opts.font_path) {
		auto sys_font = findDefaultSystemFont().get();
		opts.font_path = sys_font.file_path;
		if(sys_font.family == "Segoe UI")
			font_size_bonus = 2;
	}
	if(!opts.font_size)
		opts.font_size = (opts.style_mode == GuiStyleMode::mini ? 12 : 14) + font_size_bonus;
	m_impl->font_path = *opts.font_path;
	m_impl->font_size = *opts.font_size;

	ImGui_ImplVulkan_InitInfo info{};
	info.Instance = VulkanInstance::ref()->handle();
	info.PhysicalDevice = device->physHandle();
	info.Device = device->handle();
	auto queues = device->queues();
	info.QueueFamily = queues[0].second;
	info.Queue = queues[0].first;
	info.PipelineCache = device->pipelineCache();

	auto &rgraph = device->renderGraph();
	info.ImageCount = info.MinImageCount = rgraph.swapChain()->imageViews().size();
	info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	{
		VkDescriptorPoolSize pool_size;
		pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size.descriptorCount = 4;
		VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		ci.maxSets = 4;
		ci.poolSizeCount = 1;
		ci.pPoolSizes = &pool_size;
		if(vkCreateDescriptorPool(device, &ci, nullptr, &m_impl->descr_pool) != VK_SUCCESS)
			FATAL("vkCreateDescriptorPool failed");
		info.DescriptorPool = m_impl->descr_pool;
	}

	// TODO:
	//void (*CheckVkResultFn)(VkResult err);
	ImGui_ImplVulkan_Init(&info, rpass);

	io.KeyMap[ImGuiKey_Tab] = InputKey::tab;
	io.KeyMap[ImGuiKey_LeftArrow] = InputKey::left;
	io.KeyMap[ImGuiKey_RightArrow] = InputKey::right;
	io.KeyMap[ImGuiKey_UpArrow] = InputKey::up;
	io.KeyMap[ImGuiKey_DownArrow] = InputKey::down;
	io.KeyMap[ImGuiKey_PageUp] = InputKey::pageup;
	io.KeyMap[ImGuiKey_PageDown] = InputKey::pagedown;
	io.KeyMap[ImGuiKey_Home] = InputKey::home;
	io.KeyMap[ImGuiKey_End] = InputKey::end;
	io.KeyMap[ImGuiKey_Delete] = InputKey::del;
	io.KeyMap[ImGuiKey_Backspace] = InputKey::backspace;
	io.KeyMap[ImGuiKey_Enter] = InputKey::enter;
	io.KeyMap[ImGuiKey_Escape] = InputKey::esc;
	io.KeyMap[ImGuiKey_A] = 'a';
	io.KeyMap[ImGuiKey_C] = 'c';
	io.KeyMap[ImGuiKey_V] = 'v';
	io.KeyMap[ImGuiKey_X] = 'x';
	io.KeyMap[ImGuiKey_Y] = 'y';

	io.SetClipboardTextFn = setClipboardText;
	io.GetClipboardTextFn = getClipboardText;
	io.ClipboardUserData = nullptr;

	updateDpiAndFonts(*window, true);

	/*
#ifdef _WIN32
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	io.ImeWindowHandle = wmInfo.info.win.window;
#endif*/

	if(opts.style_mode == GuiStyleMode::mini) {
		auto &style = ImGui::GetStyle();
		style.FramePadding = {2, 1};
		style.ItemSpacing = {3, 3};
	}
}

Gui::Gui(Gui &&rhs) : m_last_time(rhs.m_last_time), m_impl(move(rhs.m_impl)) { s_instance = this; }

Gui::~Gui() {
	if(m_impl) {
		vkDeviceWaitIdle(m_impl->device);
		vkDestroyDescriptorPool(m_impl->device, m_impl->descr_pool, nullptr);
	}
	if(s_instance != this)
		return;

	s_instance = nullptr;
	ImGui_ImplVulkan_Shutdown();
	ImGui::DestroyContext();
}

bool Gui::isPresent() { return s_instance != nullptr; }
Gui &Gui::instance() {
	DASSERT(s_instance);
	return *s_instance;
}

AnyConfig Gui::config() const {
	AnyConfig out;
	string settings = ImGui::SaveIniSettingsToMemory();
	out.set("settings", settings);
	out.set("hide", o_hide);
	return out;
}

void Gui::setConfig(const AnyConfig &config) {
	if(auto *settings = config.get<string>("settings"))
		ImGui::LoadIniSettingsFromMemory(settings->c_str(), settings->size());
	o_hide = config.get("hide", false);
}

void Gui::addProcess(ProcessFunc func, void *arg) { m_impl->processes.emplace_back(func, arg); }

void Gui::removeProcess(ProcessFunc func, void *arg) {
	m_impl->processes = filter(m_impl->processes, [=](const Impl::Process &proc) {
		return proc.func != func || proc.arg != arg;
	});
}

void Gui::drawFrame(VulkanWindow &window, VkCommandBuffer cmd) {
	//PERF_GPU_SCOPE();
	if(o_hide)
		return;
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void Gui::beginFrame(VulkanWindow &window) {
	ImGuiIO &io = ImGui::GetIO();
	memset(io.KeysDown, 0, sizeof(io.KeysDown));
	memset(io.MouseDown, 0, sizeof(io.MouseDown));

	updateDpiAndFonts(window, false);

	if(!o_hide) {
		for(auto &event : window.inputEvents()) {
			if(isOneOf(event.type(), InputEventType::key_down, InputEventType::key_pressed))
				io.KeysDown[event.key()] = true;
			if(event.isMouseEvent()) {
				for(auto button : all<InputButton>)
					if(event.mouseButtonPressed(button) || event.mouseButtonDown(button))
						io.MouseDown[(int)button] = true;
			}
			if(event.keyChar())
				io.AddInputCharacter(event.keyChar()); // TODO: this is UTF16...
		}

		const auto &state = window.inputState();

		io.KeyShift = state.isKeyPressed(InputKey::lshift);
		io.KeyCtrl = state.isKeyPressed(InputKey::lctrl);
		io.KeyAlt = state.isKeyPressed(InputKey::lalt);

		// TODO: proper handling of mouse outside of current window
		// TODO: detecting short mouse clicks (single frame)
		io.MousePos = float2(state.mousePos());
		io.MouseWheel = state.mouseWheelMove();
	}

	window.showCursor(io.MouseDrawCursor && !o_hide ? false : true);

	// TODO: io.AddInputCharactersUTF8(event->text.text);

	io.DisplaySize = (ImVec2)float2(window.extent());
	// TODO:
	//io.DisplayFramebufferScale =
	//	ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

	double current_time = getTime();
	io.DeltaTime = m_last_time > 0.0 ? (float)(current_time - m_last_time) : (float)(1.0f / 60.0f);
	m_last_time = current_time;

	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();

	for(auto &proc : m_impl->processes)
		proc.func(proc.arg);
}

vector<InputEvent> Gui::finishFrame(VulkanWindow &window) {
	if(o_hide)
		return window.inputEvents();

	vector<InputEvent> out;

	ImGuiIO &io = ImGui::GetIO();
	for(auto event : window.inputEvents()) {
		if(event.isKeyEvent() && !io.WantCaptureKeyboard && !io.WantTextInput)
			out.emplace_back(event);
		if(event.isMouseEvent() && !io.WantCaptureMouse)
			out.emplace_back(event);
	}
	return out;
}
}
