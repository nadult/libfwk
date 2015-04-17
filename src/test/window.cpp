#include "fwk.h"

using namespace fwk;


unique_ptr<Renderer> s_renderer;

bool mainLoop(GfxDevice &device) { // Color nice_background(176, 215, 175);
	float3 value(1, 1, 1);

	//	drawRect(FRect(100, 100, res.x - 100, res.y - 100), {Color::green, Color::red});
	for(auto &event : device.inputEvents()) {
		if(event.keyDown(InputKey::esc) || event.type() == InputEvent::quit)
			return false;
		if(event.mouseKeyPressed(InputButton::left) || event.isMouseOverEvent()) {
			value.x = float(event.mousePos().x) / 200.0f;
			value.y = float(event.mousePos().y) / 150.0f;
		}
	}

	clear(Color(value));
	s_renderer->addRect(FRect(-100, -100, 100, 100), nullptr, Color::black);
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
	Matrix4 proj_matrix = ortho(0, res.x, 0, res.y, -1.0f, 1.0f);
	Matrix4 world_matrix = translation(0, res.y, 0) * scaling(1, -1, 1);
	s_renderer = make_unique<Renderer>(viewport, world_matrix, proj_matrix);
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
