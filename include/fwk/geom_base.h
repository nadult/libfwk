// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/tag_id.h"

namespace fwk {

DEFINE_ENUM(GeomTag, vertex, edge);

using VertexId = TagId<GeomTag::vertex>;
using EdgeId = TagId<GeomTag::edge>;

template <class T> class Contour;
template <class T> class SubContourRef;

class ImmutableGraph;
template <class T, class IT = int2> class RegularGrid;
template <class T> class SegmentGrid;
template <class T> class PlaneGraphBuilder;
template <class T> class PlaneGraph;

template <class T> using PGraph = PlaneGraph<T>;
template <class T> using PGraphBuilder = PlaneGraphBuilder<T>;

using Contour2F = fwk::Contour<float2>;
using Contour3F = fwk::Contour<float3>;

using RegularGrid2F = RegularGrid<float2>;
using RegularGrid2D = RegularGrid<double2>;

}
