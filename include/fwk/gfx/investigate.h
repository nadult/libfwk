// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include <functional>

namespace fwk {

DEFINE_ENUM(InvestigatorOpt, exit_when_finished, backtrace, exit_with_space);
using InvestigatorOpts = EnumFlags<InvestigatorOpt>;

// TODO: introspection mode ?

using VisFunc2 = std::function<string(Visualizer2 &, double2 mouse_pos)>;
using VisFunc3 = std::function<string(Visualizer3 &, double2 mouse_pos)>;

void investigate(VisFunc2 vis_func, Maybe<DRect> focus = none,
				 InvestigatorOpts = InvestigatorOpt::exit_when_finished |
									InvestigatorOpt::backtrace);

void investigate(VisFunc3 vis_func, Maybe<DBox> focus = none,
				 InvestigatorOpts = InvestigatorOpt::exit_when_finished |
									InvestigatorOpt::backtrace);

void investigateOnFail(const Expected<void> &);
}
