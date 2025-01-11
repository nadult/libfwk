// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/fwd_member.h"
#include "fwk/gfx/color.h"
#include "fwk/math/affine_trans.h"
#include "fwk/math/box.h"
#include "fwk/math/matrix4.h"
#include "fwk/vulkan/vulkan_buffer_span.h"
#include "fwk/vulkan/vulkan_pipeline.h" // TODO: includes

namespace fwk {

DEFINE_ENUM(SimpleDrawingFlag, two_sided);
using SimpleDrawingFlags = EnumFlags<SimpleDrawingFlag>;
DEFINE_ENUM(SimpleBlendingMode, disabled, normal, additive);

class SimpleMaterial {
  public:
	using DrawingFlags = SimpleDrawingFlags;
	using BlendingMode = SimpleBlendingMode;

	SimpleMaterial(PVImageView, IColor = ColorId::white, BlendingMode = BlendingMode::disabled,
				   DrawingFlags = none);
	SimpleMaterial(IColor = ColorId::white, BlendingMode = BlendingMode::disabled,
				   DrawingFlags = none);

	PVImageView texture() const { return m_texture; }
	IColor color() const { return m_color; }
	BlendingMode blendingMode() const { return m_blending_mode; }
	DrawingFlags drawingFlags() const { return m_drawing_flags; }

	FWK_ORDER_BY_DECL(SimpleMaterial);

  private:
	PVImageView m_texture;
	IColor m_color;
	BlendingMode m_blending_mode;
	DrawingFlags m_drawing_flags;
};

struct SimplePipelineSetup {
	FWK_ORDER_BY_DECL(SimplePipelineSetup);

	SimpleDrawingFlags flags = none;
	SimpleBlendingMode blending_mode = SimpleBlendingMode::disabled;
	VPrimitiveTopology primitive_topo = VPrimitiveTopology::triangle_list;
};

struct SimpleDrawCall {
	static void getVertexBindings(vector<VVertexAttrib> &, vector<VVertexBinding> &);

	static Ex<vector<PVPipeline>> makePipelines(ShaderCompiler &, VulkanDevice &, PVRenderPass,
												CSpan<SimplePipelineSetup>);

	// TODO: Add ShaderCompiler to VDevice?
	void render(VulkanDevice &);

	// Vertex data:
	// indices, tex_coords, normals & colors are optional
	VBufferSpan<u32> indices;
	VBufferSpan<float3> vertices;
	VBufferSpan<float2> tex_coords;
	VBufferSpan<IColor> colors;

	// Instance data:
	struct Instance {
		PVImageView texture;
		int pipeline_index;
		int first_index, num_vertices;
		int scissor_rect_index;
	};
	VBufferSpan<Matrix4> instance_matrices;
	vector<IRect> scissor_rects;
	vector<Instance> instances;
	vector<PVPipeline> pipelines;
};

}
