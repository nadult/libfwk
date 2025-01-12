// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/platform.h"

#ifdef FWK_PLATFORM_MSVC

#pragma comment(lib, "sdl2-static.lib")
#pragma comment(lib, "freetype.lib")
#pragma comment(lib, "libpng16_static.lib")
#pragma comment(lib, "brotlidec.lib")
#pragma comment(lib, "brotlicommon.lib")
#pragma comment(lib, "bz2.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "shaderc_shared.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "Imm32.lib")
#pragma comment(lib, "dwrite.lib")

#endif
