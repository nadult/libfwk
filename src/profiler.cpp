/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_profile.h"
#include "fwk_opengl.h"
#include <cstring>
#include <cstdio>

namespace fwk {

static Profiler *s_profiler_instance = nullptr;

Profiler::Profiler() : m_frame_count(0), m_frame_limit(0), m_last_frame_time(-1.0) {
	DASSERT(!s_profiler_instance);
	s_profiler_instance = this;
}
Profiler::~Profiler() { s_profiler_instance = nullptr; }

Profiler *Profiler::instance() { return s_profiler_instance; }

Profiler::Timer &Profiler::accessTimer(const char *name) {
	for(auto &timer : m_timers)
		if(timer.name == name)
			return timer;
	m_timers.push_back(Timer(name));
	return m_timers.back();
}

Profiler::Counter &Profiler::accessCounter(const char *name) {
	for(auto &counter : m_counters)
		if(counter.name == name)
			return counter;
	m_counters.push_back(Counter(name));
	return m_counters.back();
}

void Profiler::updateTimer(const char *id, double start_time, double end_time, bool is_rare) {
	DASSERT(end_time >= start_time);
	Timer &timer = accessTimer(id);
	timer.is_rare = is_rare;

	double time = end_time - start_time;
	if(is_rare)
		timer.values.emplace_back(m_frame_count, time);
	else
		timer.last_frame_time += time;
}

void Profiler::updateCounter(const char *id, int value) {
	Counter &counter = accessCounter(id);
	counter.value += value;
}

double Profiler::getTime() { return fwk::getTime(); }

void Profiler::openglFinish() { glFinish(); }

void Profiler::nextFrame(double expected_time) {
	for(int n = 0; n < (int)m_counters.size(); n++)
		m_counters[n].value = 0;

	m_frame_count++;
	double cur_time = getTime();
	if(cur_time - m_last_frame_time > expected_time && m_last_frame_time > 0.0) {
		int add_frames = (cur_time - m_last_frame_time) / expected_time - 1;
		if(add_frames > 0)
			m_frame_count += min(29, add_frames);
	}
	m_last_frame_time = cur_time;

	if(m_frame_count - m_frame_limit >= 30)
		m_frame_limit += 30;
}

const string Profiler::getStats(const char *filter) {
	TextFormatter out;

	double cur_time = getTime();
	long long min_frame = m_frame_limit - 30;

	for(auto &timer : m_timers) {
		double shown_value = 0.0;
		if(timer.last_frame_time > 0.0) {
			timer.values.emplace_back(m_frame_count, timer.last_frame_time);
			timer.last_frame_time = 0.0;
		}

		if(timer.values.empty()) {
			out("%s: no samples\n", timer.name.c_str());
			continue;
		}

		if(timer.is_rare) {
			shown_value = timer.values.back().second;
		} else {
			double sum = 0.0, count = 0.0;
			vector<pair<long long, double>> filtered_values;

			for(auto value : timer.values)
				if(value.first >= min_frame) {
					filtered_values.emplace_back(value);
					if(value.first < m_frame_limit) {
						sum += value.second;
						count++;
					}
				}
			shown_value = count == 0.0 ? 0.0 : sum / count;
			timer.values = move(filtered_values);
		}

		double ms = shown_value * 1000.0;
		double us = ms * 1000.0;
		bool print_ms = ms > 0.5;
		out("%s: %.2f %s\n", timer.name.c_str(), print_ms ? ms : us, print_ms ? "ms" : "us");
	}

	for(const auto &counter : m_counters) {
		if(counter.name.find(filter) != string::npos) {
			bool in_kilos = counter.value > 1000 * 10;
			out("%s: %lld%s\n", counter.name.c_str(),
				in_kilos ? (counter.value + 500) / 1000 : counter.value, in_kilos ? "k" : "");
		}
	}

	return out.text();
}

ScopedProfile::ScopedProfile(const char *id, bool is_rare, bool is_opengl)
	: id(id), is_rare(is_rare), is_opengl(is_opengl) {
	if(Profiler::instance()) {
		if(is_opengl)
			Profiler::openglFinish();
		start_time = Profiler::getTime();
	}
}

ScopedProfile::~ScopedProfile() {
	if(auto *profiler = Profiler::instance()) {
		if(is_opengl)
			Profiler::openglFinish();
		profiler->updateTimer(id, start_time, Profiler::getTime(), is_rare);
	}
}
}
