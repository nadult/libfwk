// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/menu/imgui_wrapper.h"

#include "fwk/any_config.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/menu_imgui_internal.h"
#include "fwk/sys/input.h"

#ifdef FWK_PLATFORM_HTML
#define IMGUI_IMPL_OPENGL_ES2
#endif

#include "fwk/gfx/opengl.h"
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM

#include "../extern/imgui/backends/imgui_impl_opengl3.cpp"

namespace fwk {

ImGuiWrapper *ImGuiWrapper::s_instance = nullptr;

ImGuiWrapper::ImGuiWrapper(GlDevice &device, ImGuiStyleMode style_mode) {
	ASSERT("You can only create a single instance of ImGuiWrapper" && !s_instance);
	s_instance = this;
	ImGui::CreateContext();

	// Build texture atlas
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;

	static const ImWchar glyph_ranges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin Supplement
		0x0100, 0x017F, // Latin Extended-A
		0x0180, 0x024F, // Latin Extended-B
		0x370,	0x3FF, // Greek
		0,
	};

	io.Fonts->AddFontFromFileTTF("data/LiberationSans-Regular.ttf",
								 style_mode == ImGuiStyleMode::mini ? 12 : 14, 0, glyph_ranges);
	io.FontDefault = io.Fonts->Fonts.back();
	ImGui_ImplOpenGL3_Init();

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

	/*
#ifdef _WIN32
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	io.ImeWindowHandle = wmInfo.info.win.window;
#endif*/

	if(style_mode == ImGuiStyleMode::mini) {
		auto &style = ImGui::GetStyle();
		style.FramePadding = {2, 1};
		style.ItemSpacing = {3, 3};
	}
}

ImGuiWrapper::ImGuiWrapper(ImGuiWrapper &&rhs)
	: m_last_time(rhs.m_last_time), m_procs(move(rhs.m_procs)) {
	s_instance = this;
}

ImGuiWrapper::~ImGuiWrapper() {
	if(s_instance != this)
		return;

	s_instance = nullptr;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext();
}

AnyConfig ImGuiWrapper::config() const {
	AnyConfig out;
	string settings = ImGui::SaveIniSettingsToMemory();
	out.set("settings", settings);
	out.set("hide", o_hide_menu);
	return out;
}

void ImGuiWrapper::setConfig(const AnyConfig &config) {
	if(auto *settings = config.get<string>("settings"))
		ImGui::LoadIniSettingsFromMemory(settings->c_str(), settings->size());
	o_hide_menu = config.get("hide", false);
}

void ImGuiWrapper::addProcess(ProcessFunc func, void *arg) { m_procs.emplace_back(func, arg); }

void ImGuiWrapper::removeProcess(ProcessFunc func, void *arg) {
	m_procs =
		filter(m_procs, [=](const Process &proc) { return proc.func != func || proc.arg != arg; });
}

const char *ImGuiWrapper::getClipboardText(void *) {
	static string buffer;
	buffer = GlDevice::instance().clipboardText();
	return buffer.c_str();
}

void ImGuiWrapper::setClipboardText(void *, const char *text) {
	GlDevice::instance().setClipboardText(text);
}

void ImGuiWrapper::drawFrame(GlDevice &device) {
	//PERF_GPU_SCOPE();
	if(o_hide_menu)
		return;
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiWrapper::beginFrame(GlDevice &device) {
	ImGuiIO &io = ImGui::GetIO();
	memset(io.KeysDown, 0, sizeof(io.KeysDown));
	memset(io.MouseDown, 0, sizeof(io.MouseDown));

	if(!o_hide_menu) {
		for(auto &event : device.inputEvents()) {
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

		const auto &state = device.inputState();

		io.KeyShift = state.isKeyPressed(InputKey::lshift);
		io.KeyCtrl = state.isKeyPressed(InputKey::lctrl);
		io.KeyAlt = state.isKeyPressed(InputKey::lalt);

		// TODO: proper handling of mouse outside of current window
		// TODO: detecting short mouse clicks (single frame)
		io.MousePos = float2(state.mousePos());
		io.MouseWheel = state.mouseWheelMove();
	}

	device.showCursor(io.MouseDrawCursor && !o_hide_menu ? false : true);

	// TODO: io.AddInputCharactersUTF8(event->text.text);

	io.DisplaySize = (ImVec2)float2(device.windowSize());
	// TODO:
	//io.DisplayFramebufferScale =
	//	ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

	double current_time = getTime();
	io.DeltaTime = m_last_time > 0.0 ? (float)(current_time - m_last_time) : (float)(1.0f / 60.0f);
	m_last_time = current_time;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	for(auto &proc : m_procs)
		proc.func(proc.arg);
}

vector<InputEvent> ImGuiWrapper::finishFrame(GlDevice &device) {
	if(o_hide_menu)
		return device.inputEvents();

	vector<InputEvent> out;

	ImGuiIO &io = ImGui::GetIO();
	for(auto event : device.inputEvents()) {
		if(event.isKeyEvent() && !io.WantCaptureKeyboard && !io.WantTextInput)
			out.emplace_back(event);
		if(event.isMouseEvent() && !io.WantCaptureMouse)
			out.emplace_back(event);
	}
	return out;
}
}
