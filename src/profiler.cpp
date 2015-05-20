/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_profile.h"
#include <cstring>
#include <cstdio>

namespace fwk {

namespace {

	struct Timer {
		string id;
		double value, avg;
		bool auto_clear;
	};

	struct Counter {
		string id;
		long long value;
	};

	struct StackedTimer {
		string id;
		double start_time;
	};

	vector<StackedTimer> s_stack;

	vector<Timer> s_timers;
	vector<Counter> s_counters;
	int s_frame_count = 0;
	double s_rdtsc_multiplier = 1.0;
	double s_last_frame_time[2] = {-1.0, -1.0};
}

#ifdef USE_RDTSC
double rdtscTime() {
	unsigned long long val;
		__asm__ __volatile__("rdtsc" : "=A"(val));
	return double(val) * 1.0e-9;
}

double rdtscMultiplier() { return s_rdtsc_multiplier; }
#endif

void updateTimer(const char *id, double time, bool auto_clear) {
	for(int n = 0; n < (int)s_timers.size(); n++)
		if(s_timers[n].id == id) {
			s_timers[n].auto_clear = auto_clear;
			if(auto_clear)
				s_timers[n].value += time;
			else
				s_timers[n].value = time;
			return;
		}

	s_timers.push_back(Timer{id, time, 0.0, auto_clear});
}

void updateCounter(const char *id, int value) {
	for(int n = 0; n < (int)s_counters.size(); n++)
		if(s_counters[n].id == id) {
			s_counters[n].value += value;
			return;
		}

	s_counters.push_back(Counter{id, value});
}

/*
void begin(const char *id) {
	s_stack.push_back(StackedTimer{id, 0.0f});
	s_stack.back().start_time = getProfilerTime();
}

void end(const char *id) {
	double time = getProfilerTime();
	DASSERT(!s_stack.empty() && s_stack.back().id == id);
	updateTimer(id, time - s_stack.back().start_time);
	s_stack.pop_back();
}*/

void profilerNextFrame() {
#ifdef USE_RDTSC
	// TODO: thats probably broken
	double current_time = getProfilerTime();
	double current_rtime = rdtscTime();

	if(s_last_frame_time[0] > 0.0) {
		double diff = current_time - s_last_frame_time[0];
		double rdiff = current_rtime - s_last_frame_time[1];
		s_rdtsc_multiplier = diff / rdiff;
	}

	s_last_frame_time[0] = current_time;
	s_last_frame_time[1] = current_rtime;
#endif

	for(int n = 0; n < (int)s_timers.size(); n++)
		if(s_timers[n].auto_clear) {
			s_timers[n].avg += s_timers[n].value;
			s_timers[n].value = 0.0;
		}
	for(int n = 0; n < (int)s_counters.size(); n++)
		s_counters[n].value = 0;
	s_frame_count++;
}

const string getProfilerStats(const char *filter) {
	TextFormatter out;

#ifdef USE_RDTSC
	double multiplier = s_rdtsc_multiplier;
#else
	double multiplier = 1.0;
#endif

	if(!s_timers.empty())
		out("Timers:\n");
	for(const auto &timer : s_timers) {
		double ms = timer.value * multiplier * 1000.0;
		double us = ms * 1000.0;
		bool print_ms = ms > 0.5;
		out("  %s: %.2f %s\n", timer.id.c_str(), print_ms ? ms : us, print_ms ? "ms" : "us");
	}

	if(!s_counters.empty())
		out("Counters\n");
	for(const auto &counter : s_counters) {
		if(counter.id.find(filter) != string::npos)
			out("  %s: %lld\n", counter.id.c_str(), counter.value);
	}

	return out.text();
}
}
