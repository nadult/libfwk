// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/enum_flags.h"
#include "fwk/fwd_member.h"
#include "fwk/gfx/color.h"
#include "fwk/gfx/line_buffer.h"
#include "fwk/gfx/triangle_buffer.h"
#include "fwk/gfx_base.h"
#include "fwk/math/segment.h"
#include "fwk/math_base.h"

namespace fwk {

DEFINE_ENUM(VisMode, wireframe, solid);

// DEPRECATED: use Canvas instead
class Visualizer3 {
  public:
	using Mode = VisMode;
	using ModeFlags = EnumFlags<Mode>;

	Visualizer3(float point_scale = 1.0f);
	FWK_COPYABLE_CLASS(Visualizer3);

	void clear();
	vector<SimpleDrawCall> drawCalls(bool compute_boxes = false) const;

	bool isFullyTransparent(IColor) const;

	void setMode(VisMode);
	void setMaterial(SimpleMaterial, ModeFlags = all<VisMode>);
	void setTrans(Matrix4, ModeFlags = all<VisMode>);

	void operator()(const Triangle3F &, IColor = ColorId::white);
	void operator()(const FBox &, IColor = ColorId::white);
	void operator()(const Segment3F &, IColor = ColorId::white);

	void operator()(CSpan<Triangle3F>, IColor = ColorId::white);
	void operator()(CSpan<FBox>, IColor = ColorId::white);
	void operator()(CSpan<Segment3F>, IColor = ColorId::white);

	template <class T> void operator()(const Triangle<T, 3> &tri, IColor color = ColorId::white) {
		(*this)(Triangle3F(tri), color);
	}
	template <class T> void operator()(const Box<T> &box, IColor color = ColorId::white) {
		(*this)(FBox(box), color);
	}

	template <class T, EnableIf<dim<T> == 3>...>
	void operator()(const Segment<T> &seg, IColor color = ColorId::white) {
		(*this)(Segment3F(float3(seg.from), float3(seg.to)), color);
	}

	template <class T> void operator()(CSpan<Triangle<T, 3>> span, IColor color = ColorId::white) {
		if(!isFullyTransparent(color))
			(*this)(transform<Triangle3F>(span), color);
	}
	template <class T> void operator()(CSpan<Box<T>> span, IColor color = ColorId::white) {
		if(!isFullyTransparent(color))
			(*this)(transform<FBox>(span), color);
	}

	template <class T, EnableIf<dim<T> == 3>...>
	void operator()(CSpan<Segment<T>> span, IColor color = ColorId::white) {
		if(!isFullyTransparent(color))
			(*this)(transform<Segment3F>(span), color);
	}

	void operator()(const ColoredTriangle &);
	void operator()(const ColoredQuad &);

	float pointScale() const;

  private:
	LineBuffer m_lines;
	TriangleBuffer m_tris;
	float m_point_scale;
	VisMode m_mode = VisMode::solid;
};
}
