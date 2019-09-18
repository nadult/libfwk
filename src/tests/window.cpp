// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/gl_device.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/opengl.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/sys/expected.h>
#include <fwk/sys/file_system.h>
#include <fwk/sys/input.h>

using namespace fwk;

bool mainLoop(GlDevice &device, void *font_ptr) {
	Font &font = *(Font *)font_ptr;

	static vector<float2> positions;
	static bool initializing = true;

	for(auto &event : device.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEvent::quit)
			return false;

		if(event.isMouseOverEvent() && (event.mouseMove() != int2(0, 0) || initializing))
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

	font.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, "Hello world!");
	renderer.render();

	initializing = false;
	return true;
}

Ex<Font> loadFont() {
	auto data_path = FilePath(executablePath()).parent().parent() / "data";
	auto core = EXPECT_PASS(FontCore::load(data_path / "liberation_16.fnt"));
	auto font_texture = EXPECT_PASS(GlTexture::load(data_path / "liberation_16_0.tga"));
	return Font{move(core), font_texture};
}

int main(int argc, char **argv) {
	double time = getTime();
	int2 res(800, 600);

	GlDevice gl_device;
	auto flags = GlDeviceOpt::multisampling | GlDeviceOpt::resizable | GlDeviceOpt::vsync |
				 GlDeviceOpt::opengl_debug_handler;
	gl_device.createWindow("foo", res, flags);

	print("OpenGL info:\n%\n", gl_info->toString());

	auto font = loadFont().get();
	gl_device.runMainLoop(mainLoop, &font);

	return 0;
}
