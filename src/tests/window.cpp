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
	return true;
}

string fontPath() {
#if defined(FWK_PLATFORM_WINDOWS) || defined(FWK_PLATFORM_LINUX)
	auto font_path = FilePath::current().get();
	auto start_time = getTime();
	auto sys_fonts = listSystemFonts();
	double list_time = getTime() - start_time;
	auto font_idx = findBestFont(sys_fonts, {"Segoe UI", "Liberation Sans", "Arial"}, {});
	double find_time = getTime() - start_time - list_time;
	print("List: % ms  Find: % ms\n", list_time * 1000.0, find_time * 1000.0);
	if(!font_idx)
		FWK_FATAL("Cannot find a suitable font!");
	return sys_fonts[*font_idx].file_path;
#elif FWK_PLATFORM_HTML
	auto main_path = FilePath(executablePath()).parent();
	auto font_path = main_path / "data" / "LiberationSans-Regular.ttf";
	return font_path;
#endif
}

Ex<Font> loadFont() { return FontFactory().makeFont(fontPath(), 16); }

int main(int argc, char **argv) {
	double time = getTime();
	int2 res(800, 600);

	GlDevice gl_device;
	auto flags = GlDeviceOpt::resizable | GlDeviceOpt::vsync | GlDeviceOpt::opengl_debug_handler;
	gl_device.createWindow("foo", res, {flags});

	print("OpenGL info:\n%\n", gl_info->toString());

	auto font = loadFont().get();
	gl_device.runMainLoop(mainLoop, &font);

	return 0;
}
