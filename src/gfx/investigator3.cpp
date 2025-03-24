// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigator3.h"

#include "fwk/gfx/camera_control.h"
#include "fwk/gfx/camera_variant.h"
#include "fwk/gfx/canvas_2d.h"
#include "fwk/gfx/canvas_3d.h"
#include "fwk/gfx/drawing.h"
#include "fwk/gfx/font.h"
#include "fwk/gfx/shader_compiler.h"
#include "fwk/io/file_system.h"
#include "fwk/math/constants.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/input.h"
#include "fwk/variant.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_swap_chain.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

Investigator3::Investigator3(VDeviceRef device, VWindowRef window, VisFunc3 vis_func,
							 InvestigatorOpts flags, Config config)
	: m_device(device), m_window(window), m_vis_func(vis_func), m_opts(flags) {
	m_font.emplace(Font::makeDefault(*device, window).get());
	m_compiler.emplace();

	auto new_viewport = IRect(window->size());
	Plane3F base_plane{{0, 1, 0}, {0, 1.5f, 0}};

	if(!config.focus) {
		// TODO: RenderList is not necessary here
		Canvas3D canvas(new_viewport, Matrix4::identity(), Matrix4::identity());
		m_vis_func(canvas, double2());

		FBox sum;
		bool first = false;
		//auto imatrix = inverseOrZero(canvas.viewMatrix());
		for(auto &pair : canvas.drawBoxes()) {
			auto bbox = encloseTransformed(pair.first, /*imatrix */ pair.second);
			sum = first ? bbox : enclose(sum, bbox);
			first = false;
		}

		auto size = vmax(float3(1), sum.size());
		sum = {sum.center() - size * 0.5f, sum.center() + size * 0.5f};
		config.focus = DBox(sum);
	}
	m_focus = *config.focus;

	m_cam_control = {base_plane};
	m_cam_control->o_config.params.viewport = new_viewport;

	CamVariant camera = config.camera ? *config.camera : defaultCamera();
	if(const OrbitingCamera *orb_cam = camera)
		m_cam_control->setTarget(*orb_cam);
	else if(const FppCamera *fpp_cam = camera)
		m_cam_control->setTarget(*fpp_cam);
	m_cam_control->finishAnim();

	m_cam_control->o_config.rotation_filter = [](const InputEvent &ev) {
		return ev.mouseButtonPressed(InputButton::right);
	};
	m_cam_control->o_config.move_multiplier = max(m_focus.size().values()) * 0.1f;
	m_cam_control->o_config.move_multiplier *= config.move_speed_multiplier;
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
	if(m_opts & Opt::backtrace)
		print("Running investigator:\n%s\n", Backtrace::get(2));
	m_window->runMainLoop(mainLoop, this);
	if(!m_compute_close)
		if((m_opts & Opt::exit_when_finished) || !m_exit_please)
			exit(0);
}

void Investigator3::handleInput(vector<InputEvent> events, float time_diff) {
	if(m_cam_control)
		events = m_cam_control->handleInput(events);

	bool refocus = false;
	auto exit_key = m_opts & InvestigatorOpt::exit_with_space ? InputKey::space : InputKey::esc;
	for(const auto &event : events) {
		bool shift = event.pressed(InputModifier::lshift);

		if(event.keyPressed('c'))
			refocus = true;
		if(event.keyDown(exit_key))
			m_exit_please = true;
	}

	if(refocus) {
		FppCamera cam;
		cam.focus((FBox)m_focus);
		m_cam_control->setTarget(cam);
		m_cam_control->finishAnim();
	}

	m_mouse_pos = double2(m_window->inputState().mousePos());
}

string Investigator3::draw(PVRenderPass render_pass) {
	auto viewport = IRect(m_window->size());

	m_cam_control->o_config.params.viewport = viewport;
	auto cam = m_cam_control->currentCamera();
	Canvas3D canvas(viewport, cam.projectionMatrix(), cam.viewMatrix());
	auto text = m_vis_func(canvas, m_mouse_pos);
	auto dc = canvas.genDrawCall(*m_compiler, *m_device, render_pass).get();
	dc.render(*m_device);

	return text;
}

bool Investigator3::mainLoop() {
	auto time_diff = m_window->frameTimeDiff().orElse(1.0f / 60.0f);

	handleInput(m_window->inputEvents(), time_diff);
	if(m_cam_control)
		m_cam_control->tick(time_diff, false);

	auto swap_chain = m_device->swapChain();
	IRect viewport(m_window->size());
	if(!m_depth_buffer || m_depth_buffer->size2D() != viewport.size()) {
		auto format = m_device->bestSupportedFormat(VDepthStencilFormat::d24_x8);
		auto buffer = VulkanImage::create(*m_device, VImageSetup(format, viewport.size())).get();
		m_depth_buffer = VulkanImageView::create(buffer);
	}

	m_device->beginFrame().check();
	auto screen = swap_chain->acquiredImage();
	auto depth_format = m_depth_buffer->image()->format().get<VDepthStencilFormat>();
	auto render_pass_3d = m_device->getRenderPass({screen, m_depth_buffer}, VSimpleSync::clear);
	auto render_pass_2d = m_device->getRenderPass({screen}, VSimpleSync::present);
	auto &cmds = m_device->cmdQueue();

	cmds.beginRenderPass({screen, m_depth_buffer}, render_pass_3d, none,
						 {FColor(0.4, 0.5, 0.3), VClearDepthStencil(1.0)});
	cmds.setViewport(viewport);
	cmds.setScissor(none);
	auto text = draw(render_pass_3d);
	cmds.endRenderPass();

	cmds.beginRenderPass({screen}, render_pass_2d, none);
	FontStyle style{ColorId::white, ColorId::black};
	auto extents = m_font->evalExtents(text);
	Canvas2D canvas_2d(viewport);
	canvas_2d.setViewPos(float2());
	canvas_2d.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)), FColor(0, 0, 0, 0.3));
	m_font->draw(canvas_2d, FRect({5, 5}, {300, 100}), style, text);
	canvas_2d.genDrawCall(*m_compiler, *m_device, render_pass_2d)->render(*m_device);
	cmds.endRenderPass();

	m_device->finishFrame().check();

	return !m_exit_please;
}

bool Investigator3::mainLoop(VulkanWindow &, void *this_ptr) {
	return ((Investigator3 *)this_ptr)->mainLoop();
}
}
