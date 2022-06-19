// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/gfx/shader_compiler.h>
#include <fwk/gfx/vulkan_device.h>
#include <fwk/gfx/vulkan_instance.h>
#include <fwk/gfx/vulkan_window.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>

using namespace fwk;

const char *vertex_shader = R"(
#version 450
vec2 positions[3] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
)";

const char *fragment_shader = R"(
#version 450

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

bool mainLoop(VulkanWindow &window, void *font_ptr) {
	Font &font = *(Font *)font_ptr;
	static vector<float2> positions(15, float2(window.size() / 2));

	for(auto &event : window.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEventType::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0)))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	//clearColor(IColor(50, 0, 50));
	/*Renderer2D renderer(IRect(VkDevice::instance().windowSize()), Orient2D::y_down);

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	auto text = format("Hello world!\nWindow size: %", device.windowSize());
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);
	renderer.render();*/

	return true;
}

string fontPath() {
	if(platform == Platform::html)
		return "data/LiberationSans-Regular.ttf";
	return findDefaultSystemFont().get().file_path;
}

Ex<int> exMain() {
	double time = getTime();

	VulkanInstance instance;
	{
		VulkanInstanceConfig config;
		config.debug_levels = VDebugLevel::warning | VDebugLevel::error;
		config.debug_types = all<VDebugType>;
		instance.initialize(config).check();
	}

	// TODO: making sure that shaderc_shared.dll is available
	ShaderCompiler compiler;
	auto vsh_compiled = compiler.compile(ShaderType::vertex, vertex_shader);
	auto fsh_compled = compiler.compile(ShaderType::fragment, fragment_shader);

	auto flags = VWindowFlag::resizable | VWindowFlag::vsync | VWindowFlag::centered |
				 VWindowFlag::allow_hidpi;
	auto window = EX_PASS(construct<VulkanWindow>("fwk::vulkan_test", IRect(0, 0, 1280, 720),
												  VulkanWindowConfig{flags}));

	auto pref_device = instance.preferredDevice(window.surfaceHandle());
	if(!pref_device)
		return ERROR("Coudln't find a suitable Vulkan device");
	auto device = EX_PASS(instance.makeDevice(*pref_device));

	EXPECT(window.createSwapChain(device));

	int font_size = 16 * window.dpiScale();
	//auto font = FontFactory().makeFont(fontPath(), font_size);
	window.runMainLoop(mainLoop, nullptr); //&font.get());

	window.destroySwapChain();

	return 0;
}

int main(int argc, char **argv) {
	auto result = exMain();
	if(!result) {
		result.error().print();
		return 1;
	}
	return *result;
}
