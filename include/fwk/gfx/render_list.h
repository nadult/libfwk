// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"

namespace fwk {

class RenderList : public MatrixStack {
  public:
	RenderList(const IRect &viewport, const Matrix4 &projection_matrix = Matrix4::identity());
	RenderList(const RenderList &) = delete;
	void operator=(const RenderList &) = delete;
	~RenderList();

	void render();
	void clear();

	void add(DrawCall);
	void add(DrawCall, const Matrix4 &);
	void add(CSpan<DrawCall>);
	void add(CSpan<DrawCall>, const Matrix4 &);

	const auto &drawCalls() const { return m_draw_calls; }

	// Draw calls without bbox argument specified will be ignored
	vector<pair<FBox, Matrix4>> renderBoxes() const;

  protected:
	vector<DrawCall> m_draw_calls;
	IRect m_viewport;
};
}
