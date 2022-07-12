// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"

#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_storage.h" // TODO: only ptr needed
#include "fwk/vulkan_base.h"

namespace fwk {

DEFINE_ENUM(BlendingMode, normal, additive);

class SimpleMaterial {
  public:
	SimpleMaterial(PVImageView texture, FColor color = ColorId::white,
				   BlendingMode bm = BlendingMode::normal)
		: m_texture(texture), m_color(color), m_blendingMode(bm) {}
	SimpleMaterial(FColor color = ColorId::white, BlendingMode bm = BlendingMode::normal)
		: m_color(color), m_blendingMode(bm) {}

	PVImageView texture() const { return m_texture; }
	FColor color() const { return m_color; }
	BlendingMode blendingMode() const { return m_blendingMode; }

  private:
	PVImageView m_texture;
	FColor m_color;
	BlendingMode m_blendingMode;
};

class Renderer2D : public MatrixStack {
  public:
	Renderer2D(const IRect &viewport, Orient2D);
	~Renderer2D();

	void setViewPos(const float2 &view_pos);
	void setViewPos(const int2 &view_pos) { setViewPos(float2(view_pos)); }

	struct VulkanPipelines {
		vector<PVPipeline> pipelines;
		PVImageView white;
		PVSampler sampler;
		IRect viewport;
	};

	static Ex<VulkanPipelines> makeVulkanPipelines(VDeviceRef, VColorAttachment, IRect);

	struct DrawCall {
		PVBuffer vbuffer, ibuffer, matrix_buffer;
	};

	Ex<DrawCall> genDrawCall(VDeviceRef, const VulkanPipelines &);
	Ex<void> render(DrawCall &, VDeviceRef, const VulkanPipelines &);

	void addFilledRect(const FRect &rect, const FRect &tex_rect, CSpan<FColor, 4>,
					   const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const FRect &tex_rect, const SimpleMaterial &);
	void addFilledRect(const FRect &rect, const SimpleMaterial &mat) {
		addFilledRect(rect, FRect({1, 1}), mat);
	}
	void addFilledRect(const IRect &rect, const SimpleMaterial &mat) {
		addFilledRect(FRect(rect), mat);
	}
	void addFilledEllipse(float2 center, float2 size, FColor, int num_tris = 32);

	void addRect(const FRect &rect, FColor);
	void addRect(const IRect &rect, FColor color) { addRect(FRect(rect), color); }
	void addEllipse(float2 center, float2 size, FColor, int num_edges = 32);

	void addLine(const float2 &, const float2 &, FColor);
	void addLine(const int2 &p1, const int2 &p2, FColor color) {
		addLine(float2(p1), float2(p2), color);
	}

	struct Element {
		Matrix4 matrix;
		PVImageView texture;
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
	vector<Pair<FRect, Matrix4>> renderRects() const;

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
	Element &makeElement(DrawChunk &, PrimitiveType, PVImageView,
						 BlendingMode = BlendingMode::normal);

	vector<DrawChunk> m_chunks;
	vector<IRect> m_scissor_rects;
	IRect m_viewport;
	int m_current_scissor_rect;
};
}
