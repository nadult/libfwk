// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/gfx_base.h"

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

class LineBuffer {
  public:
	struct Instance {
		Matrix4 matrix;
		vector<float3> positions;
		vector<IColor> colors;
		MaterialFlags material_flags;
		IColor material_color;
	};

	LineBuffer(const MatrixStack &);
	void add(CSpan<float3>, CSpan<IColor>, const Material &,
			 const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<float3>, const Material &, const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<float3>, IColor, const Matrix4 &matrix = Matrix4::identity());
	void add(CSpan<Segment3<float>>, const Material &, const Matrix4 &matrix = Matrix4::identity());
	void addBox(const FBox &bbox, IColor color, const Matrix4 &matrix = Matrix4::identity());
	void clear();

	const auto &instances() const { return m_instances; }

  private:
	Instance &instance(IColor, MaterialFlags, Matrix4, bool has_colors);

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
	const auto &lines() const { return m_lines; }
	auto &sprites() { return m_sprites; }
	auto &lines() { return m_lines; }

	// Draw calls without bbox argument specified will be ignored
	vector<pair<FBox, Matrix4>> renderBoxes() const;

  protected:
	void renderLines();
	void renderSprites();

	vector<DrawCall> m_draw_calls;
	SpriteBuffer m_sprites;
	LineBuffer m_lines;
	IRect m_viewport;
};
}
