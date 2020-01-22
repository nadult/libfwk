// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// ImGuiWrapper originated from dear imgui SDL+Opengl3 example
//
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.
// TODO: use binding from Opengl 3

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include "fwk/menu/imgui_wrapper.h"

#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/opengl.h"
#include "fwk/io/xml.h"
#include "fwk/menu_imgui_internal.h"
#include "fwk/sys/input.h"

namespace fwk {

ImGuiWrapper *ImGuiWrapper::s_instance = nullptr;

struct ImGuiWrapper::Internals {
	Internals() = default;
	Ex<void> exConstruct() {
		auto vsh_source = "uniform mat4 ProjMtx;\n"
						  "in vec2 Position;\n"
						  "in vec2 UV;\n"
						  "in vec4 Color;\n"
						  "out vec2 Frag_UV;\n"
						  "out vec4 Frag_Color;\n"
						  "void main() {\n"
						  "	Frag_UV = UV;\n"
						  "	Frag_Color = Color;\n"
						  "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
						  "}\n";

		auto fsh_source = "uniform sampler2D Texture;\n"
						  "in mediump vec2 Frag_UV;\n"
						  "in lowp vec4 Frag_Color;\n"
						  "out lowp vec4 Out_Color;\n"
						  "void main() {\n"
						  "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
						  "}\n";

		auto version = gl_info->profile == GlProfile::es ? "#version 300 es\n" : "#version 330\n";

		auto vsh = EX_PASS(GlShader::make(ShaderType::vertex, {version, vsh_source}));
		auto fsh = EX_PASS(GlShader::make(ShaderType::fragment, {version, fsh_source}));
		program = EX_PASS(GlProgram::make(vsh, fsh));

		AttribLocationTex = program->location("Texture");
		AttribLocationProjMtx = program->location("ProjMtx");

		glGenBuffers(1, &VboHandle);
		glGenBuffers(1, &ElementsHandle);

		glGenVertexArrays(1, &VaoHandle);
		glBindVertexArray(VaoHandle);
		glBindBuffer(GL_ARRAY_BUFFER, VboHandle);
		glEnableVertexAttribArray(AttribLocationPosition);
		glEnableVertexAttribArray(AttribLocationUV);
		glEnableVertexAttribArray(AttribLocationColor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t) & (((TYPE *)0)->ELEMENT))
		glVertexAttribPointer(AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
							  (GLvoid *)OFFSETOF(ImDrawVert, pos));
		glVertexAttribPointer(AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
							  (GLvoid *)OFFSETOF(ImDrawVert, uv));
		glVertexAttribPointer(AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert),
							  (GLvoid *)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		testGlError("Initializing imgui shaders");

		return {};
	}

	PProgram program;
	int AttribLocationTex = 0, AttribLocationProjMtx = 0;
	int AttribLocationPosition = 0, AttribLocationUV = 1, AttribLocationColor = 2;
	unsigned int VboHandle = 0, VaoHandle = 0, ElementsHandle = 0;
};

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
		0x370,  0x3FF, // Greek
		0,
	};

	io.Fonts->AddFontFromFileTTF("data/LiberationSans-Regular.ttf",
								 style_mode == ImGuiStyleMode::mini ? 12 : 14, 0, glyph_ranges);
	io.FontDefault = io.Fonts->Fonts.back();

	unsigned char *pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Upload texture to graphics system
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &m_font_tex);
	glBindTexture(GL_TEXTURE_2D, m_font_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// Store our identifier
	io.Fonts->TexID = (void *)(intptr_t)m_font_tex;

	// Restore state
	glBindTexture(GL_TEXTURE_2D, last_texture);

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

	auto internals = construct<Internals>();
	internals.check();
	m_internals = move(*internals);
}

ImGuiWrapper::ImGuiWrapper(ImGuiWrapper &&rhs)
	: m_last_time(rhs.m_last_time), m_procs(move(rhs.m_procs)), m_font_tex(rhs.m_font_tex) {
	rhs.m_font_tex = 0;
	s_instance = this;
}

ImGuiWrapper::~ImGuiWrapper() {
	if(s_instance != this)
		return;

	s_instance = nullptr;

	if(m_font_tex) {
		glDeleteTextures(1, &m_font_tex);
		ImGui::GetIO().Fonts->TexID = 0;
		m_font_tex = 0;
	}
	ImGui::DestroyContext();
}

void ImGuiWrapper::saveSettings(XmlNode xnode) const {
	auto &settings = GImGui->SettingsWindows;
	for(auto &elem : settings) {
		auto enode = xnode.addChild("window");
		enode("name") = enode.own(elem.Name);
		enode("pos") = (int2)elem.Pos;
		enode("size") = (int2)elem.Size;
		enode("collapsed", false) = elem.Collapsed;
	}
	xnode("hide", false) = o_hide_menu;
}

void ImGuiWrapper::loadSettings(CXmlNode xnode) {
	auto &settings = GImGui->SettingsWindows;
	settings.clear();
	auto enode = xnode.child("window");
	while(enode) {
		auto name = enode.attrib("name");
		if(!anyOf(settings,
				  [&](const ImGuiWindowSettings &elem) { return Str(elem.Name) == name; })) {
			ImGuiWindowSettings elem;
			elem.Name = ImStrdup(ZStr(enode.attrib("name")).c_str());
			elem.ID = ImHashStr(elem.Name);
			elem.Pos = enode.attrib<int2>("pos");
			elem.Size = enode.attrib<int2>("size");
			elem.Collapsed = xnode.attrib("collapsed", false);
			if(!exceptionRaised())
				settings.push_back(elem);
			else
				printExceptions();
		}
		enode.next();
	}
	o_hide_menu = xnode.attrib("hide", false);
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
	auto *draw_data = ImGui::GetDrawData();

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	ImGuiIO &io = ImGui::GetIO();
	int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if(fb_width == 0 || fb_height == 0)
		return;
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	// Backup GL state
	glActiveTexture(GL_TEXTURE0);
	GLint last_viewport[4];
	glGetIntegerv(GL_VIEWPORT, last_viewport);
	GLint last_scissor_box[4];
	glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
	GLenum last_blend_src_rgb;
	glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
	GLenum last_blend_dst_rgb;
	glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
	GLenum last_blend_src_alpha;
	glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
	GLenum last_blend_dst_alpha;
	glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
	GLenum last_blend_equation_rgb;
	glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
	GLenum last_blend_equation_alpha;
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);

	GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
	GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
	GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	// Setup viewport, orthographic projection matrix
	glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
	const float ortho_projection[4][4] = {
		{2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f},
		{0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f},
		{0.0f, 0.0f, -1.0f, 0.0f},
		{-1.0f, 1.0f, 0.0f, 1.0f},
	};

	auto *ctx = m_internals.get();
	ctx->program->use();
	glUniform1i(ctx->AttribLocationTex, 0);
	glUniformMatrix4fv(ctx->AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
	glBindVertexArray(ctx->VaoHandle);
	glBindSampler(0, 0); // Rely on combined texture/sampler state.

	for(int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList *cmd_list = draw_data->CmdLists[n];
		const ImDrawIdx *idx_buffer_offset = 0;

		glBindBuffer(GL_ARRAY_BUFFER, ctx->VboHandle);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert),
					 (const GLvoid *)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ElementsHandle);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
					 (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx),
					 (const GLvoid *)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

		for(int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
			if(pcmd->UserCallback) {
				pcmd->UserCallback(cmd_list, pcmd);
			} else {
				glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
				glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w),
						  (int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
						  (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
				glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
							   sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
							   idx_buffer_offset);
			}
			idx_buffer_offset += pcmd->ElemCount;
		}
	}

	// Restore modified GL state
	glUseProgram(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindSampler(0, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
	glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha,
						last_blend_dst_alpha);

	if(last_enable_blend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	if(last_enable_cull_face)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	if(last_enable_depth_test)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	if(last_enable_scissor_test)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2],
			   (GLsizei)last_viewport[3]);
	glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2],
			  (GLsizei)last_scissor_box[3]);
	testGlError("imgui_stuff");
}

void ImGuiWrapper::beginFrame(GlDevice &device) {
	ImGuiIO &io = ImGui::GetIO();
	memset(io.KeysDown, 0, sizeof(io.KeysDown));
	memset(io.MouseDown, 0, sizeof(io.MouseDown));

	if(!o_hide_menu) {
		for(auto &event : device.inputEvents()) {
			if(isOneOf(event.type(), InputEvent::key_down, InputEvent::key_pressed))
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
