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
	using DrawingFlag = SimpleDrawingFlag;
	using DrawingFlags = SimpleDrawingFlags;
	using BlendingMode = SimpleBlendingMode;

	SimpleMaterial(PVImageView, IColor = ColorId::white, DrawingFlags = none,
				   BlendingMode = BlendingMode::disabled);
	SimpleMaterial(IColor = ColorId::white, DrawingFlags = none,
				   BlendingMode = BlendingMode::disabled);

	PVImageView texture() const { return m_texture; }
	IColor color() const { return m_color; }
	DrawingFlags drawingFlags() const { return m_drawing_flags; }
	BlendingMode blendingMode() const { return m_blending_mode; }

	FWK_ORDER_BY_DECL(SimpleMaterial);

  private:
	PVImageView m_texture;
	IColor m_color;
	DrawingFlags m_drawing_flags;
	BlendingMode m_blending_mode;
};

struct SimplePipelineSetup {
	SimpleDrawingFlags flags;
	SimpleBlendingMode blending_mode;
	VPrimitiveTopology primitive_topo;
};

struct SimpleDrawCall {
	static void getVertexBindings(vector<VVertexAttrib> &, vector<VVertexBinding> &);

	static Ex<vector<PVPipeline>> makePipelines(ShaderCompiler &, VulkanDevice &, PVRenderPass,
												CSpan<SimplePipelineSetup>);

	// TODO: Add ShaderCompiler to VDevice?
	void render(VulkanDevice &);

	// Jaki render_pass ?
	// Rendering jest po prostu zawsze do pierwszego outputu? Render pass jako parametr?

	/*
	static Ex<SimpleDrawCall> make(VulkanDevice &, CSpan<float3> vertices,
									  CSpan<IColor> colors = {}, CSpan<float2> tex_coords = {},
									  CSpan<float3> normals = {}, CSpan<u32> indices = {},
									  VPrimitiveTopology = VPrimitiveTopology::triangle_list,
									  VMemoryUsage mem_usage = VMemoryUsage::device);
	static Ex<SimpleDrawCall> make(VulkanDevice &, CSpan<ColoredTriangle>,
									  VMemoryUsage mem_usage = VMemoryUsage::device);*/

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
	};
	VBufferSpan<Matrix4> instance_matrices;
	vector<Instance> instances;
	vector<PVPipeline> pipelines;
};

// Potrzebuj� funkcj� do narysowania zestawu draw-calli:
//
// Czy potrzebuj� jeszcze jakiego� cache-a na pipeline-y ?
//
// funkcja jako argumentu bierze te� ShaderCompiler;
// compiler jest inicjowany odpowiednimi definicjami, je�li nie istniej�
//
// jak zrobi� cache ? i gdzie ten cache by�by trzymany ?
// W VulkanDevice ?
//
// device.getCachedPipeline(compiler, function, params) ?
// funkcja by�aby zahashowana ? czy mogliby�my tam przekaza� jaki� stan ?
//
// To jest ok
//
// Po skompilowaniu pipeline�w, musze si� jeszcze mi�dzy nimi szybko prze��cza�;
// Czy do tego potrzebuj� hash mapy ? Bo czasami r�nych tryb�w mo�e by� pierdyliard...
// zwyk�a numeracja od 0 nie zadzia�a...
//
// Mo�e m�g�bym zwraca� HashMap� z gotowymi pipeline-ami ?
// Albo zwraca� wska�nik na cache bazuj�cy na konkretnej funkcji

// Chc� mie� jeden system renderingu dla 2D i 3D?
// Jeden SimpleVertexArray powinien wystarczy�
// Renderer2D m�g�by u�ywa� tego samego systemu do renderingu: nie musi by� zajrbi�cie szybki

// Draw Calle nie s� potrzebne?
// Mo�e niech zostan�: przdadz� si� tez do renderingu meshy, etc.?

}
