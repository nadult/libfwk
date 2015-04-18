#include "fwk.h"

using namespace fwk;

unique_ptr<Renderer> s_renderer;

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

	clear(Color(50, 0, 50));
	for(int n = 0; n < (int)positions.size(); n++) {
		FRect rect = FRect(-50, -50, 50, 50) + positions[n];
		Color fill_color(float4(1.0f - n * 0.1f, 1.0f - n * 0.05f, 0, 1.0f));
		Color border_color = Color::black;

		s_renderer->addRect(rect, nullptr, {fill_color, border_color});
	}

	s_renderer->render();

	return true;
}

int safe_main(int argc, char **argv) {
	double time = getTime();
	int2 res(640, 480);

	auto &gfx_device = GfxDevice::instance();
	gfx_device.createWindow(res, false);

	// setBlendingMode(bmDisabled);
	IRect viewport(res);
	s_renderer = make_unique<Renderer>(viewport);
	gfx_device.runMainLoop(mainLoop);

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
}
