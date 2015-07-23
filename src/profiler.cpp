/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_profile.h"
#include <cstring>
#include <cstdio>

namespace fwk {

namespace {

	struct Timer {
		Timer(const string &name) : name(name), is_rare(false) {}

		string name;
		vector<pair<double, double>> values;
		double last_average;
		bool is_rare;
	};

	struct Counter {
		Counter(const string &name) : name(name), value(0) {}

		string name;
		long long value;
	};

	vector<Timer> s_timers;
	vector<Counter> s_counters;
	int s_frame_count = 0;
	double s_last_frame_time[2] = {-1.0, -1.0};
	double s_last_limit = 0.0;

	Timer &accessTimer(const string &name) {
		for(auto &timer : s_timers)
			if(timer.name == name)
				return timer;
		s_timers.push_back(Timer(name));
		return s_timers.back();
	}

	Counter &accessCounter(const string &name) {
		for(auto &counter : s_counters)
			if(counter.name == name)
				return counter;
		s_counters.push_back(Counter(name));
		return s_counters.back();
	}
}

void updateTimer(const char *id, double start_time, double end_time, bool is_rare) {
	DASSERT(end_time >= start_time);
	Timer &timer = accessTimer(id);
	timer.is_rare = is_rare;
	timer.values.emplace_back(start_time, end_time);
}

void updateCounter(const char *id, int value) {
	Counter &counter = accessCounter(id);
	counter.value += value;
}

double getProfilerTime() { return getTime(); }

void profilerNextFrame() {
	for(int n = 0; n < (int)s_counters.size(); n++)
		s_counters[n].value = 0;
	s_frame_count++;
}

const string getProfilerStats(const char *filter) {
	TextFormatter out;

	double cur_time = getTime();
	if(s_last_limit < cur_time) {
		if(s_last_limit + 10.0 < cur_time)
			s_last_limit = cur_time;
		while(s_last_limit < cur_time - 0.5)
			s_last_limit += 0.5;
	}
	double min_time = s_last_limit - 1.0;

	if(!s_timers.empty())
		out("Timers:\n");
	for(auto &timer : s_timers) {
		double shown_value = 0.0;

		if(timer.values.empty()) {
			out("  %s: no samples\n", timer.name.c_str());
			continue;
		}

		if(timer.is_rare) {
			shown_value = timer.values.back().second - timer.values.back().first;
		} else {
			double sum = 0.0, count = 0.0;
			vector<pair<double, double>> filtered_values;

			for(auto value : timer.values)
				if(value.second >= min_time) {
					filtered_values.emplace_back(value);
					if(value.second < s_last_limit) {
						sum += value.second - value.first;
						count ++;
					}
				}
			shown_value = sum / count;
			timer.values = std::move(filtered_values);
		}

		double ms = shown_value * 1000.0;
		double us = ms * 1000.0;
		bool print_ms = ms > 0.5;
		out("  %s: %.2f %s\n", timer.name.c_str(), print_ms ? ms : us, print_ms ? "ms" : "us");
	}

	if(!s_counters.empty())
		out("Counters\n");
	for(const auto &counter : s_counters) {
		if(counter.name.find(filter) != string::npos) {
			bool in_kilos = counter.value > 1000 * 10;
			out("  %s: %lld%s\n", counter.name.c_str(),
				in_kilos ? (counter.value + 500) / 1000 : counter.value, in_kilos ? "k" : "");
		}
	}

	return out.text();
}
}
