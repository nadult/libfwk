// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigator3.h"

#include "fwk/gfx/camera_control.h"
#include "fwk/gfx/camera_variant.h"
#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/font_factory.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/render_list.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk/gfx/visualizer3.h"
#include "fwk/io/file_system.h"
#include "fwk/math/constants.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/input.h"
#include "fwk/variant.h"

#ifndef FWK_IMGUI_DISABLED
#include "fwk/menu/imgui_wrapper.h"
#endif

namespace fwk {

Investigator3::Investigator3(VisFunc vis_func, Maybe<DBox> focus, InvestigatorFlags flags,
							 MaybeCRef<CamVariant> cam)
	: m_vis_func(vis_func), m_flags(flags) {
	assertGlThread();

	auto charset = FontFactory::ansiCharset() + FontFactory::basicMathCharset();
	// TODO: hard-code default font somehow? Keep only minimal set of glyphs?
	auto main_path = FilePath(executablePath()).parent();
	auto font_path = main_path / "data" / "LiberationSans-Regular.ttf";
	m_font.emplace(move(FontFactory().makeFont(font_path, charset, 14, false).get()));

	auto new_viewport = IRect(GlDevice::instance().windowSize());
	Plane3F base_plane{{0, 1, 0}, {0, 1.5f, 0}};

	if(!focus) {
		// TODO: RenderList is not necessary here
		RenderList temp(IRect(GlDevice::instance().windowSize()), Matrix4::identity());
		Visualizer3 vis(1.0f);
		m_vis_func(vis, double2());
		temp.add(vis.drawCalls(false));

		FBox sum;
		bool first = false;
		auto imatrix = inverseOrZero(temp.viewMatrix());
		for(auto &pair : temp.renderBoxes()) {
			auto bbox = encloseTransformed(pair.first, imatrix * pair.second);
			sum = first ? bbox : enclose(sum, bbox);
			first = false;
		}

		auto size = vmax(float3(1), sum.size());
		sum = {sum.center() - size * 0.5f, sum.center() + size * 0.5f};
		focus = DBox(sum);
	}
	m_focus = *focus;

	if(!cam)
		cam = defaultCamera();

	m_cam_control = {base_plane};
	m_cam_control->o_config.params.viewport = new_viewport;

	if(const OrbitingCamera *orb_cam = *cam)
		m_cam_control->setTarget(*orb_cam);
	else if(const FppCamera *fpp_cam = *cam)
		m_cam_control->setTarget(*fpp_cam);
	m_cam_control->finishAnim();

	m_cam_control->o_config.rot_filter = [](const InputEvent &ev) {
		return ev.mouseButtonPressed(InputButton::right);
	};
	m_cam_control->o_config.move_multiplier = max(m_focus.size().values()) * 0.1f;
	m_cam_control->o_config.params.depth = {0.125f, 1024.0f};
}

Investigator3::~Investigator3() = default;

auto Investigator3::defaultCamera() const -> CamVariant {
	FppCamera cam;
	cam.focus((FBox)m_focus);
	return cam;
}

auto Investigator3::camera() const -> CamVariant { return m_cam_control->target(); }

void Investigator3::run() {
	if(m_flags & Opt::backtrace)
		print("Running investigator:\n%s\n", Backtrace::get(2));

	GlDevice::instance().runMainLoop(mainLoop, this);

	if(!m_compute_close)
		if((m_flags & Opt::exit_when_finished) || !m_esc_pressed)
			exit(0);
}

void Investigator3::handleInput(GlDevice &device, vector<InputEvent> events, float time_diff) {
	if(m_cam_control)
		events = m_cam_control->handleInput(events);

	bool refocus = false;
	for(const auto &event : events) {
		bool shift = event.pressed(InputModifier::lshift);

		if(event.keyPressed('c'))
			refocus = true;
		if(event.keyDown(InputKey::esc))
			m_esc_pressed = true;
	}

	if(refocus) {
		FppCamera cam;
		cam.focus((FBox)m_focus);
		m_cam_control->setTarget(cam);
		m_cam_control->finishAnim();
	}

	m_mouse_pos = double2(device.inputState().mousePos());
}

void Investigator3::draw() {
	auto viewport = IRect(GlDevice::instance().windowSize());
	Renderer2D renderer_2d(viewport, Orient2D::y_down);

	TextFormatter fmt;

	m_cam_control->o_config.params.viewport = viewport;
	auto cam = m_cam_control->currentCamera();
	RenderList renderer_3d(viewport, cam.projectionMatrix());
	renderer_3d.setViewMatrix(cam.viewMatrix());

	Visualizer3 vis(1.0f);
	auto text = m_vis_func(vis, m_mouse_pos);
	renderer_3d.add(vis.drawCalls(false));
	fmt << text;
	renderer_3d.render();

	FontStyle style{ColorId::white, ColorId::black};
	auto extents = m_font->evalExtents(fmt.text());
	renderer_2d.setViewPos(float2());
	renderer_2d.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)),
							  {IColor(0, 0, 0, 80)});

	m_font->draw(renderer_2d, FRect({5, 5}, {300, 100}), style, fmt.text());

	renderer_2d.render();
}

bool Investigator3::mainLoop(GlDevice &device) {
	IColor nice_background(100, 120, 80);
	clearColor(nice_background);
	clearDepth(1.0f);

	float time_diff = 1.0f / 60.0f;

	vector<InputEvent> events;
#ifndef FWK_IMGUI_DISABLED
	auto *imgui = ImGuiWrapper::instance();
	if(imgui) {
		imgui->beginFrame(device);
		events = imgui->finishFrame(device);
	} else {
		events = device.inputEvents();
	}
#else
	events = device.inputEvents();
#endif

	handleInput(device, events, time_diff);
	if(m_cam_control)
		m_cam_control->tick(time_diff, false);

	draw();
#ifndef FWK_IMGUI_DISABLED
	if(imgui)
		imgui->drawFrame(device);
#endif

	return !m_esc_pressed;
}

bool Investigator3::mainLoop(GlDevice &device, void *this_ptr) {
	return ((Investigator3 *)this_ptr)->mainLoop(device);
}
}
