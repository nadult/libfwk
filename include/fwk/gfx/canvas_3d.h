// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"
#include "fwk/gfx/color.h"
#include "fwk/gfx/drawing.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/math/box.h"
#include "fwk/pod_vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

// TODO: fix point & line drawing (direction and scale)
// TODO: add functionality from Visualizer3
// TODO: feeding directly to command buffer would be faster

class Canvas3D {
  public:
	using TexCoord = float2;

	Canvas3D(const IRect &viewport, const Matrix4 &proj_matrix, const Matrix4 &view_matrix);
	FWK_COPYABLE_CLASS(Canvas3D);

	Ex<SimpleDrawCall> genDrawCall(ShaderCompiler &, VulkanDevice &, PVRenderPass,
								   VMemoryUsage = VMemoryUsage::frame);

	vector<Pair<FBox, Matrix4>> drawBoxes() const;

	// --------------------------------------------------------------------------------------------
	// ---------- Changing canvas state -----------------------------------------------------------

	void setPointWidth(float width);
	void setSegmentWidth(float width);
	float pointWidth() const { return m_point_width; }
	float segmentWidth() const { return m_segment_width; }

	void setMaterial(const SimpleMaterial &);
	SimpleMaterial getMaterial() const;

	void pushViewMatrix();
	void popViewMatrix();
	void mulViewMatrix(const Matrix4 &);
	void setViewMatrix(const Matrix4 &);

	const IRect &viewport() const { return m_viewport; }

	// --------------------------------------------------------------------------------------------
	// ------------ Drawing functions -------------------------------------------------------------

	void addTris(CSpan<float3>, CSpan<TexCoord> = {}, CSpan<IColor> = {});
	void addQuads(CSpan<float3>, CSpan<TexCoord> = {}, CSpan<IColor> = {});
	void addPoints(CSpan<float3>, CSpan<IColor> = {});
	void addSegments(CSpan<float3>, CSpan<IColor> = {});

	void addSegment(const float3 &, const float3 &, FColor);
	void addSegment(const int3 &p1, const int3 &p2, FColor color) {
		addSegment(float3(p1), float3(p2), color);
	}

	// TODO: advanced drawing functions from Visualizer2

  private:
	struct Group {
		Group(int first_index, int pipeline_index)
			: first_index(first_index), num_indices(0), pipeline_index(pipeline_index) {}

		PVImageView texture;
		int first_index, num_indices;
		int pipeline_index;
	};

	int getPipeline(const SimplePipelineSetup &);
	bool splitGroup();

	void appendColors(CSpan<IColor>, int num_vertices, int multiplier);
	void appendQuadTexCoords(CSpan<TexCoord>, int num_vertices);
	void appendQuadIndices(int first_vertex, int num_quads);

	IRect m_viewport;
	MatrixStack m_matrix_stack;
	vector<SimplePipelineSetup> m_pipelines;
	vector<Matrix4> m_group_matrices;
	vector<Group> m_groups;

	PodVector<float3> m_positions;
	PodVector<TexCoord> m_tex_coords;
	PodVector<IColor> m_colors;
	PodVector<u32> m_indices;

	float m_point_width = 1.0f, m_segment_width = 1.0f;
	FColor m_cur_color = ColorId::white;
};
}
