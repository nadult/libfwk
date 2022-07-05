// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/font_factory.h>
#include <fwk/gfx/font_finder.h>
#include <fwk/gfx/gl_device.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/opengl.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/io/file_system.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/input.h>

using namespace fwk;

void updateFPS(GlDevice &device) {
	static double prev_time = getTime();
	static int num_frames = 0;
	double cur_time = getTime();
	num_frames++;
	if(cur_time - prev_time > 1.0) {
		int fps = double(num_frames) / (cur_time - prev_time);
		device.setWindowTitle(format("fwk::vulkan_test [FPS: %]", fps));
		prev_time = cur_time;
		num_frames = 0;
	}
}

bool mainLoop(GlDevice &device, void *font_ptr) {
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

	clearColor(IColor(50, 0, 50));
	Renderer2D renderer(IRect(GlDevice::instance().windowSize()), Orient2D::y_down);

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	auto text = format("Hello world!\nWindow size: %", device.windowSize());
	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, text);
	renderer.render();
	updateFPS(device);

	return true;
}

string fontPath() {
	if(platform == Platform::html)
		return "data/LiberationSans-Regular.ttf";
	return findDefaultSystemFont().get().file_path;
}

int main(int argc, char **argv) {
	double time = getTime();

	GlDevice gl_device;
	auto display_rects = gl_device.displayRects();
	if(!display_rects)
		FWK_FATAL("No display available");
	int2 res = display_rects[0].size() / 2;
	auto flags = GlDeviceOpt::resizable | GlDeviceOpt::opengl_debug_handler |
				 GlDeviceOpt::centered | GlDeviceOpt::allow_hidpi;
	gl_device.createWindow("foo", IRect(res), {flags});
	print("OpenGL info:\n%\n", gl_info->toString());

	int font_size = 16 * gl_device.windowDpiScale();
	Font font(FontFactory().makeFont(fontPath(), font_size).get());
	gl_device.runMainLoop(mainLoop, &font);

	return 0;
}
