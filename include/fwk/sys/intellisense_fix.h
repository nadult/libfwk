// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file contains fixes for Intellisense parsing issues when using together with clang-cl.
// These issues show up in latest version (November 2025) of Visual Studio 2022, but are possibly
// fixed in VS 2026.

// Source: https://developercommunity.visualstudio.com/t/A-lot-of-false-positives-when-using-clan/10839013

#pragma once

#if defined(__INTELLISENSE__) && defined(__clang__) && defined(_MSC_VER)

// These fix Intellisense parsing problems that affect many system/library headers
#define __bf16 unsigned short
#define __building_module(x) (0)

// To prevent use of __builtin_FUNCSIG() in std::source_location::current()
#define _USE_DETAILED_FUNCTION_NAME_IN_SOURCE_LOCATION 0

#endif
