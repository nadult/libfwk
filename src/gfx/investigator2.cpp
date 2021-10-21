// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/investigator2.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/font_factory.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/render_list.h"
#include "fwk/gfx/triangle_buffer.h"
#include "fwk/gfx/visualizer2.h"
#include "fwk/io/file_system.h"
#include "fwk/math/constants.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/input.h"

namespace fwk {
Investigator2::Investigator2(VisFunc2 vis_func, Maybe<DRect> focus, InvestigatorOpts flags)
	: m_vis_func(vis_func), m_opts(flags) {
	assertGlThread();

	if(!focus) {
		auto pscale = 1.0 / (m_scale * m_view_scale);

		Visualizer2 vis(pscale, pscale);
		m_vis_func(vis, double2());

		auto is_finite = [](float3 pt) {
			return allOf(pt.values(), [](float v) { return v >= -inf && v < inf; });
		};

		FRect sum;
		for(auto pair : vis.drawBoxes())
			if(is_finite(pair.first.min()) && is_finite(pair.first.max()))
				sum = encloseNotEmpty(sum, pair.first.xy());

		// In case we're focusing on a single point
		auto size = vmax(float2(1), sum.size());
		sum = {sum.center() - size * 0.5f, sum.center() + size * 0.5f};
		focus = DRect(sum);
	}
	m_focus = *focus;

	auto data_path = FilePath(executablePath()).parent() / "data";
	auto font_path = data_path / "LiberationSans-Regular.ttf";
	auto charset = FontFactory::ansiCharset() + FontFactory::basicMathCharset();
	m_font.emplace(move(FontFactory().makeFont(font_path, charset, 14, false).get()));
}

Investigator2::~Investigator2() = default;

void Investigator2::run() {
	if(m_opts & Opt::backtrace)
		print("Running investigator:\n%s\n", Backtrace::get(2));

	GlDevice::instance().runMainLoop(mainLoop, this);

	if((m_opts & Opt::exit_when_finished) || !m_exit_please)
		exit(1);
}

void Investigator2::handleInput(GlDevice &device, float time_diff) {
	double2 view_move;
	double scale_mod = 0.0;

	auto events = device.inputEvents();
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

	m_mouse_pos = double2(device.inputState().mousePos());
}

vector<Vis2Label> Investigator2::draw(RenderList &out, TextFormatter &fmt) {
	auto viewport = out.viewport();
	auto cursor_pos =
		double2(m_mouse_pos - viewport.center()) * double2(1, -1) / (m_scale * m_view_scale) +
		m_view_pos;
	string text;

	vector<Vis2Label> labels;

	{
		float3 center(float2(viewport.center()), 0.0f);
		out.pushViewMatrix();
		out.mulViewMatrix(translation(center));
		float3 offset(-float2(m_view_pos), 0.0f);
		out.mulViewMatrix(scaling(float3(1, -1, 1) * m_scale * m_view_scale) * translation(offset));
		auto pscale = 1.0 / (m_scale * m_view_scale);

		Visualizer2 vis(pscale, pscale);
		text = m_vis_func(vis, cursor_pos);
		out.add(vis.drawCalls());
		auto matrix = out.viewMatrix();
		out.popViewMatrix();
		labels = vis.labels();
	}

	fmt("Investigating...\n%", text);
	return labels;
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

void Investigator2::draw() {
	RenderList rlist(m_viewport, projectionMatrix2D(m_viewport, Orient2D::y_down));
	rlist.setViewMatrix(viewMatrix2D(m_viewport, float2()));

	TextFormatter fmt;

	auto labels = draw(rlist, fmt);

	FontStyle style{ColorId::white, ColorId::black};
	auto extents = m_font->evalExtents(fmt.text());
	Visualizer2 vis;

	float scale = m_scale * m_view_scale;
	float2 offset = -float2(m_view_pos);
	for(auto &label : labels) {
		float2 rect_min = label.rect.min() + offset, rect_max = label.rect.max() + offset;
		rect_min.y *= -1.0f, rect_max.y *= -1.0f;
		FRect rect = FRect(rect_min, rect_max) * scale + float2(rlist.viewport().center());

		FontStyle style{label.style.color, negativeColor(label.style.color), HAlign::center,
						VAlign::center};
		m_font->draw(vis.triangleBuffer(), rect, style, label.text);
	}

	vis.drawRect(FRect(float2(extents.size()) + float2(10, 10)), {IColor(0, 0, 0, 80)}, true);
	FRect text_rect = FRect({5, 5}, {300, 100});
	m_font->draw(vis.triangleBuffer(), text_rect, style, fmt.text());

	rlist.add(vis.drawCalls());
	rlist.render(true);
}

bool Investigator2::mainLoop(GlDevice &device) {
	IColor nice_background(100, 120, 80);
	clearColor(nice_background);
	clearDepth(1.0f);

	float time_diff = 1.0f / 60.0f;

	m_viewport = (IRect)device.windowSize();
	m_view_scale = min(m_viewport.width(), m_viewport.height());

	handleInput(device, time_diff);
	applyFocus();
	draw();

	return !m_exit_please;
}

bool Investigator2::mainLoop(GlDevice &device, void *this_ptr) {
	return ((Investigator2 *)this_ptr)->mainLoop(device);
}
}
