#include "fwk.h"

using namespace fwk;

bool mainLoop(GfxDevice &device) {
	static vector<float2> positions;

	for(auto &event : device.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEvent::quit)
			return false;

		if(event.isMouseOverEvent() && event.mouseMove() != int2(0, 0))
			positions.emplace_back(float2(event.mousePos()));
	}

	while(positions.size() > 15)
		positions.erase(positions.begin());

	GfxDevice::clearColor(Color(50, 0, 50));
	Renderer2D renderer(IRect(GfxDevice::instance().windowSize()));

	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect(-50, -50, 50, 50) + positions[n];
		Color fill_color(float4(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f));
		Color border_color = Color::black;

		renderer.addFilledRect(rect, fill_color);
		renderer.addRect(rect, border_color);
	}

	static PFont font;
	static PTexture font_texture;
	if(!font) {
		Loader font_ldr("data/liberation_16.fnt");
		Loader tex_ldr("data/liberation_16_0.tga");
		font = make_immutable<Font>("", font_ldr);
		font_texture = make_immutable<DTexture>("", tex_ldr);
	}
	FontRenderer font_renderer(font, font_texture, renderer);
	font_renderer.draw(FRect(5, 5, 200, 20), {Color::white}, "Hello world!");

	renderer.render();

	return true;
}

int safe_main(int argc, char **argv) {
	double time = getTime();
	int2 res(800, 600);

	GfxDevice gfx_device;
	gfx_device.createWindow("foo", res, GfxDevice::flag_multisampling | GfxDevice::flag_resizable |
											GfxDevice::flag_vsync);
	gfx_device.runMainLoop(mainLoop);

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), ex.backtrace().c_str());
		return 1;
	}
}
