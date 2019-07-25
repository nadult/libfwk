// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/menu_base.h"
#include "fwk/math_base.h"

#define IM_ASSERT(_EXPR)  DASSERT(_EXPR)

#define IM_VEC2_CLASS_EXTRA                                                      \
        ImVec2(const fwk::float2& f) { x = f.x; y = f.y; }                       \
        ImVec2(const fwk::double2& f) { x = f.x; y = f.y; }                      \
        ImVec2(const fwk::int2& f) { x = f.x; y = f.y; }                         \
        operator fwk::float2() const { return {x,y}; }                           \
        operator fwk::double2() const { return {x,y}; }                          \
        operator fwk::int2() const { return fwk::int2(x,y); }                    \
        ImVec2 operator+(const ImVec2 &rhs) { return {x + rhs.x, y + rhs.y}; }   \
        ImVec2 operator-(const ImVec2 &rhs) { return {x - rhs.x, y - rhs.y}; }

#define IM_VEC4_CLASS_EXTRA                                                      \
        ImVec4(const fwk::float4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }     \
        ImVec4(const fwk::int4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }       \
        ImVec4(const fwk::double4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }    \
        operator fwk::float4() const { return {x,y,z,w}; }                       \
        operator fwk::double4() const { return {x,y,z,w}; }                      \
        operator fwk::int4() const { return fwk::int4(x,y,z,w); }

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "../extern/imgui/imgui.h"

