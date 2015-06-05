/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_PROFILE_H
#define FWK_PROFILE_H

#include "fwk_base.h"

namespace fwk {

//TODO: Add hierarchical timers
void updateTimer(const char *name, double start_time, double end_time, bool is_rare = true);
void updateCounter(const char *name, int value);
double getProfilerTime();
void profilerNextFrame();

const string getProfilerStats(const char *filter = "");

struct AutoTimer {
	AutoTimer(const char *id, bool is_rare)
		: start_time(getProfilerTime()), id(id), is_rare(is_rare) {}
	~AutoTimer() { updateTimer(id, start_time, getProfilerTime(), is_rare); }

	double start_time;
	const char *id;
	bool is_rare;
};

#define FWK_PROFILE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, false)
#define FWK_PROFILE_RARE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, true)

#define FWK_PROFILE_NAME(a) FWK_PROFILE_NAME_(a)
#define FWK_PROFILE_NAME_(a) timer##a
}

#endif
