// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

// This header includes everything
// You can use it for convenience, but it comes with a cost

#include "fwk/sys_base.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/error.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/memory.h"
#include "fwk/sys/rollback.h"
#include "fwk/sys/stream.h"

#include "fwk/cstring.h"
#include "fwk/format.h"

#include "fwk_input.h"
#include "fwk/math_base.h"
#include "fwk_maybe.h"
#include "fwk_mesh.h"
#include "fwk_parse.h"
#include "fwk_profile.h"
#include "fwk_variant.h"
#include "fwk/sys/xml.h"

#include "fwk/math/random.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/dtexture.h"
#include "fwk/gfx/font.h"
#include "fwk/gfx/frame_buffer.h"
#include "fwk/gfx/gfx_device.h"
#include "fwk/gfx/index_buffer.h"
#include "fwk/gfx/material_set.h"
#include "fwk/gfx/matrix_stack.h"
#include "fwk/gfx/program_binder.h"
#include "fwk/gfx/render_buffer.h"
#include "fwk/gfx/render_list.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk/gfx/shader.h"
#include "fwk/gfx/texture.h"
#include "fwk/gfx/vertex_array.h"
#include "fwk/gfx/vertex_buffer.h"
