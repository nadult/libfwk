#pragma once

#ifndef _WIN32
#error This file should only be included on Windows platform
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef max
#undef min
#undef ERROR