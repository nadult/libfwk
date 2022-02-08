// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#ifdef __linux__
#define FWK_PLATFORM_LINUX 1
#endif

#if _MSC_VER
#define FWK_PLATFORM_MSVC 1
#define FWK_DWARF_DISABLED
#endif

#ifdef __MINGW32__
#define FWK_PLATFORM_MINGW 1
#endif

#ifdef __EMSCRIPTEN__
#define FWK_PLATFORM_HTML 1
#endif

namespace fwk {
enum class Platform { linux, mingw, msvc, html };
#if defined(FWK_PLATFORM_LINUX)
inline constexpr auto platform = Platform::linux;
#elif defined(FWK_PLATFORM_MINGW)
inline constexpr auto platform = Platform::mingw;
#elif defined(FWK_PLATFORM_MSVC)
inline constexpr auto platform = Platform::msvc;
#else
inline constexpr auto platform = Platform::html;
#endif
}
