// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/str.h"
#include "fwk/vector.h"

namespace fwk {

enum SystemFontStyle { normal, oblique, italic };

struct SystemFontParams {
	int weight = 400; // Range: 100 - 950
	int stretch = 0; // Range: 0 - 10 (0: undefined)
	SystemFontStyle style = SystemFontStyle::normal;
};

struct SystemFont {
	string family;
	string style;
	string file_path;
	SystemFontParams params;
};

vector<SystemFont> listSystemFonts();
// family_names: preferred font family names (ordered by priority)
// params: preferred font params
// best matching font index will be returned or none
Maybe<int> findBestFont(CSpan<SystemFont> input_fonts, CSpan<Str> family_names,
						SystemFontParams params);

// Co jeszcze jest potrzebne ?
// znajdowanie konkretnego fonta ? najlepszego dopasowania ?

}