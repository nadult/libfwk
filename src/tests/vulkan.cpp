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

bool mainLoop(VulkanDevice &device, void *font_ptr) {
	Font &font = *(Font *)font_ptr;
	static vector<float2> positions(15, float2(device.windowSize() / 2));

	for(auto &event : device.inputEvents()) {
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

int main(int argc, char **argv) {
	double time = getTime();

	VulkanInstance vinstance;
	{
		VulkanInstanceConfig config;
		config.debug_levels = VDebugLevel::warning | VDebugLevel::error;
		config.debug_types = all<VDebugType>;
		vinstance.initialize(config).check();
	}

	ShaderCompiler compiler;
	auto vsh_compiled = compiler.compile(ShaderType::vertex, vertex_shader);
	auto fsh_compled = compiler.compile(ShaderType::fragment, fragment_shader);

	VulkanDevice vdevice;
	auto display_rects = vdevice.displayRects();
	if(!display_rects)
		FWK_FATAL("No display available");
	int2 res = display_rects[0].size() / 2;
	auto flags = VDeviceFlag::resizable | VDeviceFlag::vsync | VDeviceFlag::validation |
				 VDeviceFlag::centered | VDeviceFlag::allow_hidpi;
	vdevice.createWindow("fwk::vulkan_test", IRect(res), {flags});

	int font_size = 16 * vdevice.windowDpiScale();
	//auto font = FontFactory().makeFont(fontPath(), font_size);
	vdevice.runMainLoop(mainLoop, nullptr); //&font.get());

	return 0;
}
