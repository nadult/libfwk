// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"

namespace fwk {

class Renderer2D : public MatrixStack {
  public:
	Renderer2D(const IRect &viewport);
	~Renderer2D();

	// Simple 2D view has (0, 0) in top left corner of the screen
	static Matrix4 simpleProjectionMatrix(const IRect &viewport);
	static Matrix4 simpleViewMatrix(const IRect &viewport, const float2 &view_pos);

	void setViewPos(const float2 &view_pos);
	void setViewPos(const int2 &view_pos) { setViewPos(float2(view_pos)); }

	void render();

	void addFilledRect(const FRect &rect, const FRect &tex_rect, CSpan<FColor, 4>,
					   const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const FRect &tex_rect, const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const SimpleMaterial &mat) {
		addFilledRect(rect, FRect({1, 1}), mat);
	}
	void addFilledRect(const IRect &rect, const SimpleMaterial &mat) {
		addFilledRect(FRect(rect), mat);
	}

	void addRect(const FRect &rect, FColor);
	void addRect(const IRect &rect, FColor color) { addRect(FRect(rect), color); }

	void addLine(const float2 &, const float2 &, FColor);
	void addLine(const int2 &p1, const int2 &p2, FColor color) {
		addLine(float2(p1), float2(p2), color);
	}

	struct Element {
		Matrix4 matrix;
		shared_ptr<const DTexture> texture;
		int first_index, num_indices;
		int scissor_rect_id;
		PrimitiveType primitive_type;
		BlendingMode blending_mode;
	};

	// tex_coord & color can be empty
	void addQuads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>,
				  const SimpleMaterial &);
	void addLines(CSpan<float2> pos, CSpan<FColor>, FColor mat_color);
	void addTris(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>,
				 const SimpleMaterial &material);

	Maybe<IRect> scissorRect() const;
	void setScissorRect(Maybe<IRect>);

	void clear();
	const IRect &viewport() const { return m_viewport; }
	vector<pair<FRect, Matrix4>> renderRects() const;

  private:
	struct DrawChunk {
		void appendVertices(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>, FColor);

		vector<float2> positions;
		vector<float2> tex_coords;
		vector<IColor> colors;
		vector<int> indices;
		vector<Element> elements;
	};

	DrawChunk &allocChunk(int num_verts);
	Element &makeElement(DrawChunk &, PrimitiveType, shared_ptr<const DTexture>,
						 BlendingMode = BlendingMode::normal);

	vector<DrawChunk> m_chunks;
	vector<IRect> m_scissor_rects;
	IRect m_viewport;
	PProgram m_tex_program, m_flat_program;
	int m_current_scissor_rect;
};
}
