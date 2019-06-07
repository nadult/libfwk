// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/menu/helpers.h"

namespace menu {

namespace impl {
	bool inputValue(const char *label, int &value) { return ImGui::InputInt(label, &value); }
	bool inputValue(const char *label, float &value) { return ImGui::InputFloat(label, &value); }
}

void showTooltip(Str str) {
	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(450.0f);
	text(str);
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

void showHelpMarker(Str text, const char *marker) {
	ImGui::TextDisabled("%s", marker);
	if(ImGui::IsItemHovered())
		showTooltip(text);
}

void centeredText(int center_pos, Str str) {
	auto tsize = ImGui::CalcTextSize(str.begin(), str.end());
	ImGui::Dummy({center_pos - tsize.x * 0.5f, 1.0f});
	ImGui::SameLine();
	ImGui::TextUnformatted(str.begin(), str.end());
}

void text(Str str) { ImGui::TextUnformatted(str.begin(), str.end()); }
}
