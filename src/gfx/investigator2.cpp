// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigator2.h"

#include "fwk/gfx/canvas_2d.h"
#include "fwk/gfx/drawing.h"
#include "fwk/gfx/font.h"
#include "fwk/gfx/shader_compiler.h"
#include "fwk/io/file_system.h"
#include "fwk/math/constants.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/input.h"
#include "fwk/vulkan/vulkan_command_queue.h"
#include "fwk/vulkan/vulkan_device.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_swap_chain.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {
Investigator2::Investigator2(VDeviceRef device, VWindowRef window, VisFunc2 vis_func,
							 Maybe<DRect> focus, InvestigatorOpts flags)
	: m_device(device), m_window(window), m_vis_func(vis_func), m_opts(flags) {

	m_compiler.emplace();

	if(!focus) {
		// TODO: pscale is invalid
		auto pscale = 1.0 / (m_scale * m_view_scale);

		Canvas2D canvas(IRect(window->size()));
		canvas.setPointWidth(pscale);
		canvas.setSegmentWidth(pscale);
		m_vis_func(canvas, double2());

		auto is_finite = [](float2 pt) {
			return allOf(pt.values(), [](float v) { return v >= -inf && v < inf; });
		};

		FRect sum;
		for(auto pair : canvas.drawRects())
			if(is_finite(pair.first.min()) && is_finite(pair.first.max()))
				sum = encloseNotEmpty(sum, pair.first);

		// In case we're focusing on a single point
		auto size = vmax(float2(1), sum.size());
		sum = {sum.center() - size * 0.5f, sum.center() + size * 0.5f};
		focus = DRect(sum);
	}

	m_focus = *focus;
	m_font.emplace(Font::makeDefault(*device, window).get());
}

Investigator2::~Investigator2() = default;

void Investigator2::run() {
	if(m_opts & Opt::backtrace)
		print("Running investigator:\n%s\n", Backtrace::get(2));

	m_window->runMainLoop(mainLoop, this);

	if((m_opts & Opt::exit_when_finished) || !m_exit_please)
		exit(1);
}

void Investigator2::handleInput(float time_diff) {
	double2 view_move;
	double scale_mod = 0.0;

	auto events = m_window->inputEvents();
	auto exit_key = m_opts & InvestigatorOpt::exit_with_space ? InputKey::space : InputKey::esc;
	for(const auto &event : events) {
		bool shift = event.pressed(InputModifier::lshift);

		if(event.keyPressed(InputKey::left))
			view_move.x -= time_diff * 2.0;
		if(event.keyPressed(InputKey::right))
			view_move.x += time_diff * 2.0;
		if(event.keyPressed(InputKey::up))
			view_move.y += time_diff * 2.0;
		if(event.keyPressed(InputKey::down))
			view_move.y -= time_diff * 2.0;
		if(event.keyPressed(InputKey::pageup))
			scale_mod += time_diff * 2.0;
		if(event.keyPressed(InputKey::pagedown))
			scale_mod -= time_diff * 2.0;
		if(event.keyPressed('c'))
			m_view_pos = {};
		if(event.keyDown(exit_key))
			m_exit_please = true;
	}

	if(scale_mod != 0.0)
		m_scale = clamp(m_scale * (1.0 + scale_mod), 0.000000000001, 1000000000000.0);
	m_view_pos += view_move * 200.0 / (m_view_scale * m_scale);

	m_mouse_pos = double2(m_window->inputState().mousePos());
}

void Investigator2::draw(Canvas2D &canvas, TextFormatter &fmt) {
	auto viewport = canvas.viewport();
	auto cursor_pos =
		double2(m_mouse_pos - viewport.center()) * double2(1, -1) / (m_scale * m_view_scale) +
		m_view_pos;
	float3 center(float2(viewport.center()), 0.0f);

	canvas.pushViewMatrix();
	canvas.mulViewMatrix(translation(center));
	float3 offset(-float2(m_view_pos), 0.0f);
	canvas.mulViewMatrix(scaling(float3(1, -1, 1) * m_scale * m_view_scale) * translation(offset));
	auto old_point_width = canvas.pointWidth();
	canvas.setPointWidth(1.0 / (m_scale * m_view_scale));
	auto text = m_vis_func(canvas, cursor_pos);
	canvas.setPointWidth(old_point_width);
	canvas.popViewMatrix();

	fmt("Investigating...\n%", text);
}

void Investigator2::applyFocus() {
	if(m_focus_applied)
		return;

	m_view_pos = m_focus.center();
	m_scale = 0.9 / max(m_focus.width(), m_focus.height());
	m_focus_applied = true;
}

static IColor negativeColor(IColor color) {
	auto rgb = FColor(color).rgb();
	auto col1 = lerp(rgb, float3(0.0f), 0.8f), col2 = lerp(rgb, float3(1.0f), 0.8f);
	return (IColor)FColor(distanceSq(col1, rgb) > distanceSq(col2, rgb) ? col1 : col2);
}

void Investigator2::draw(PVRenderPass render_pass) {
	Canvas2D canvas(m_viewport);
	TextFormatter fmt;
	draw(canvas, fmt);

	FontStyle style{ColorId::white, ColorId::black};
	auto extents = m_font->evalExtents(fmt.text());

	// TODO: labels
	/*
	float scale = m_scale * m_view_scale;
	float2 offset = -float2(m_view_pos);
	for(auto &label : labels) {
		float2 rect_min = label.rect.min() + offset, rect_max = label.rect.max() + offset;
		rect_min.y *= -1.0f, rect_max.y *= -1.0f;
		FRect rect = FRect(rect_min, rect_max) * scale + float2(rlist.viewport().center());

		FontStyle style{label.style.color, negativeColor(label.style.color), HAlign::center,
						VAlign::center};
		m_font->draw(vis.triangleBuffer(), rect, style, label.text);
	}*/

	canvas.addFilledRect(FRect(float2(extents.size()) + float2(10, 10)), IColor(0, 0, 0, 80));
	FRect text_rect = FRect({5, 5}, {300, 100});
	m_font->draw(canvas, text_rect, style, fmt.text());

	auto dc = canvas.genDrawCall(*m_compiler, *m_device, render_pass).get();
	dc.render(*m_device);
}

bool Investigator2::mainLoop() {
	m_viewport = IRect(m_window->size());
	m_view_scale = min(m_viewport.width(), m_viewport.height());
	auto time_diff = m_window->frameTimeDiff().orElse(1.0f / 60.0f);
	handleInput(time_diff);
	applyFocus();

	m_device->beginFrame().check();
	auto screen = m_device->swapChain()->acquiredImage();
	auto render_pass = m_device->getRenderPass({screen}, VSimpleSync::clear_present);

	auto &cmds = m_device->cmdQueue();
	cmds.beginRenderPass({screen}, render_pass, none, {FColor(0.4, 0.5, 0.3)});
	cmds.setViewport(m_viewport);
	cmds.setScissor(none);
	draw(render_pass);
	cmds.endRenderPass();
	m_device->finishFrame().check();

	return !m_exit_please;
}

bool Investigator2::mainLoop(VulkanWindow &, void *this_ptr) {
	return ((Investigator2 *)this_ptr)->mainLoop();
}
}
