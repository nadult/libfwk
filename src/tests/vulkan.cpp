// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/gfx/vk_device.h>
#include <fwk/gfx/vulkan.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>

// Jak lepiej nazywa� klasy ?
// Chc� mie� na razie zar�wo obs�ug� VK & OpenGL ?
// Po co mi OGL ?

using namespace fwk;

bool mainLoop(fwk::VkDevice &device, void *font_ptr) {
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

	fwk::VkDevice vk_device;
	auto display_rects = vk_device.displayRects();
	if(!display_rects)
		FWK_FATAL("No display available");
	int2 res = display_rects[0].size() / 2;
	auto flags = VkDeviceOpt::resizable | VkDeviceOpt::vsync | VkDeviceOpt::validation |
				 VkDeviceOpt::centered | VkDeviceOpt::allow_hidpi;
	vk_device.createWindow("fwk::vulkan_test", IRect(res), {flags});
	print("Vulkan info:\n%\n", vk_info->toString());

	int font_size = 16 * vk_device.windowDpiScale();
	//auto font = FontFactory().makeFont(fontPath(), font_size);
	vk_device.runMainLoop(mainLoop, nullptr); //&font.get());

	return 0;
}