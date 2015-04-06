/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_PROFILE_H
#define FWK_PROFILE_H

#include "fwk_base.h"

//#define USE_RDTSC

namespace fwk {


void updateTimer(const char *name, double time, bool auto_clear = true);
void updateCounter(const char *name, int value);
void profilerNextFrame();

void begin(const char *name);
void end(const char *name);

const string getProfilerStats(const char *filter = "");

#ifdef USE_RDTSC
double rdtscTime();
double rdtscMultiplier();
inline double getProfilerTime() { return rdtscTime(); }
#else
inline double getProfilerTime() { return getTime(); }
#endif

struct AutoTimer {
#ifdef USE_RDTSC
	AutoTimer(const char *id, bool auto_clear)
		: time(rdtscTime()), id(id), auto_clear(auto_clear) {}
	~AutoTimer() { updateTimer(id, rdtscTime() - time, auto_clear); }
#else
	AutoTimer(const char *id, bool auto_clear)
		: time(getProfilerTime()), id(id), auto_clear(auto_clear) {}
	~AutoTimer() { updateTimer(id, getProfilerTime() - time, auto_clear); }
#endif

	double time;
	const char *id;
	bool auto_clear;
};

#define FWK_PROFILE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, true)
#define FWK_PROFILE_RARE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, false)

#define FWK_PROFILE_NAME(a) FWK_PROFILE_NAME_(a)
#define FWK_PROFILE_NAME_(a) timer##a
}

#ifdef USE_RDTSC
#undef USE_RDTSC
#endif

#endif
