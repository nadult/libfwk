// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"

namespace fwk {

// TODO:
// RenderList -> CommandList (albo nawet brak specjalnej klasy?)
// Wywalenie Renderer2D, używanie jednego systemu render listy
// TriangleBuffer & LineBuffer generują listę komend; Możliwość generacji
// naprzemiennych komend
//
class RenderList : public MatrixStack {
  public:
	RenderList(const IRect &viewport, const Matrix4 &projection_matrix = Matrix4::identity());
	RenderList(const RenderList &) = delete;
	void operator=(const RenderList &) = delete;
	~RenderList();

	void render(bool mode_2d = false);
	void clear();

	void add(DrawCall);
	void add(DrawCall, const Matrix4 &);
	void add(CSpan<DrawCall>);
	void add(CSpan<DrawCall>, const Matrix4 &);

	const auto &drawCalls() const { return m_draw_calls; }
	const IRect &viewport() const { return m_viewport; }

	// Draw calls without bbox argument specified will be ignored
	vector<pair<FBox, Matrix4>> renderBoxes() const;

  protected:
	vector<DrawCall> m_draw_calls;
	IRect m_viewport;
};
}
