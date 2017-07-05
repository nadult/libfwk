// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk.h"

using namespace fwk;

bool mainLoop(GfxDevice &device, void *) {
	static vector<float2> positions;

	for(auto &event : device.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEvent::quit)
			return false;

		if(event.isMouseOverEvent() && event.mouseMove() != int2(0, 0))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	GfxDevice::clearColor(IColor(50, 0, 50));
	Renderer2D renderer(IRect(GfxDevice::instance().windowSize()));

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
		Loader font_ldr("data/liberation_16.fnt");
		Loader tex_ldr("data/liberation_16_0.tga");
		font_core = make_immutable<FontCore>("", font_ldr);
		font_texture = make_immutable<DTexture>("", tex_ldr);
	}
	Font(font_core, font_texture)
		.draw(renderer, FRect({5, 5}, {200, 20}), {ColorId::white}, "Hello world!");

	renderer.render();

	return true;
}

int safe_main(int argc, char **argv) {
	double time = getTime();
	int2 res(800, 600);

	GfxDevice gfx_device;
	auto flags = GfxDevice::flag_multisampling | GfxDevice::flag_resizable | GfxDevice::flag_vsync;
	gfx_device.createWindow("foo", res, flags);
	gfx_device.runMainLoop(mainLoop);

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n", ex.what());
		return 1;
	}
}
