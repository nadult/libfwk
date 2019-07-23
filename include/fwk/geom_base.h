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

using Contour2F = fwk::Contour<float2>;
using Contour3F = fwk::Contour<float3>;

}
