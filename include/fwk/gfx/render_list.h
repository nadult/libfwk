// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"

namespace fwk {

class SpriteBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		Material material;
		vector<float3> positions;
		vector<float2> tex_coords;
		vector<IColor> colors;
	};

	SpriteBuffer(const MatrixStack &);
	void add(CSpan<float3> verts, CSpan<float2> tex_coords, CSpan<IColor> colors, const Material &,
			 const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }

  private:
	Instance &instance(const Material &, Matrix4, bool has_colors, bool has_tex_coords);

	vector<Instance> m_instances;
	const MatrixStack &m_matrix_stack;
};

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
	const auto &sprites() const { return m_sprites; }
	auto &sprites() { return m_sprites; }

	// Draw calls without bbox argument specified will be ignored
	vector<pair<FBox, Matrix4>> renderBoxes() const;

  protected:
	void renderSprites();

	vector<DrawCall> m_draw_calls;
	SpriteBuffer m_sprites;
	IRect m_viewport;
};
}
