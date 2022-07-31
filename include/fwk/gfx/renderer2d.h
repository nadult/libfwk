// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx/drawing.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"
#include "fwk/vulkan/vulkan_pipeline.h"
#include "fwk/vulkan/vulkan_storage.h" // TODO: only ptr needed
#include "fwk/vulkan_base.h"

namespace fwk {

class Renderer2D : public MatrixStack {
  public:
	Renderer2D(const IRect &viewport, Orient2D);
	~Renderer2D();

	Ex<SimpleDrawCall> genDrawCall(ShaderCompiler &, VulkanDevice &, PVRenderPass,
								   VMemoryUsage = VMemoryUsage::frame);

	void setViewPos(const float2 &view_pos);
	void setViewPos(const int2 &view_pos) { setViewPos(float2(view_pos)); }

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

	struct BasicMaterial {
		BasicMaterial(const SimpleMaterial &);
		bool operator==(const BasicMaterial &) const;

		PVImageView texture;
		SimpleBlendingMode blending_mode;
	};

	struct Element {
		BasicMaterial material;
		int first_index, num_indices;
		VPrimitiveTopology prim_topo;
	};

	// tex_coord & color can be empty
	void addQuads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>,
				  const SimpleMaterial &);
	void addLines(CSpan<float2> pos, CSpan<FColor>, FColor mat_color);
	void addTris(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>,
				 const SimpleMaterial &material);

	const IRect &viewport() const { return m_viewport; }
	vector<Pair<FRect, Matrix4>> drawRects() const;

  private:
	void appendVertices(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor>, FColor);
	Element &makeElement(VPrimitiveTopology, const SimpleMaterial &);

	vector<float3> m_positions;
	vector<float2> m_tex_coords;
	vector<IColor> m_colors;
	vector<u32> m_indices;
	vector<Matrix4> m_matrices;
	vector<Element> m_elements;
	IRect m_viewport;
};
}
