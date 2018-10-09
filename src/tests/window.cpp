// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include <fwk/filesystem.h>
#include <fwk/format.h>
#include <fwk/gfx/font.h>
#include <fwk/gfx/gl_device.h>
#include <fwk/gfx/gl_texture.h>
#include <fwk/gfx/opengl.h>
#include <fwk/gfx/renderer2d.h>
#include <fwk/sys/input.h>
#include <fwk/sys/stream.h>

using namespace fwk;

bool mainLoop(GlDevice &device, void *) {
	static vector<float2> positions;

	for(auto &event : device.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEvent::quit)
			return false;

		if(event.isMouseOverEvent() && event.mouseMove() != int2(0, 0))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	clearColor(IColor(50, 0, 50));
	Renderer2D renderer(IRect(GlDevice::instance().windowSize()));

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect({-50, -50}, {50, 50}) + positions[n];
		FColor fill_color(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f);
		IColor border_color = ColorId::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	static PFontCore font_core;
	static PTexture font_texture;
	if(!font_core) {
		auto data_path = FilePath(executablePath()).parent().parent() / "data";
		Loader font_ldr(data_path / "liberation_16.fnt");
		Loader tex_ldr(data_path / "liberation_16_0.tga");
		font_core = make_immutable<FontCore>("", font_ldr);
		font_texture.emplace("", tex_ldr);
	}
	Font(font_core, font_texture)
		.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, "Hello world!");

	renderer.render();

	return true;
}

int main(int argc, char **argv) {
	double time = getTime();
	int2 res(800, 600);

	GlDevice gl_device;
	auto flags = GlDeviceOpt::multisampling | GlDeviceOpt::resizable | GlDeviceOpt::vsync |
				 GlDeviceOpt::opengl_debug_handler;
	gl_device.createWindow("foo", res, flags);

	print("OpenGL info:\n%\n", gl_info->toString());
	gl_device.runMainLoop(mainLoop);

	return 0;
}
