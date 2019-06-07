// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace ImGui {};

namespace menu {
using namespace fwk;
using namespace ImGui;

void openFilePopup(string &file_name, ZStr popup_name, string file_regex = ".*");
void openFileButton(string &file_name, string popup_name, string regex = ".*");

void openErrorPopup(Error);
void displayErrorPopup();

void showTooltip(Str);
void showHelpMarker(Str, const char *marker = "(?)");
}

namespace fwk {
class ImGuiWrapper;
}
