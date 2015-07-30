/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_PROFILE_H
#define FWK_PROFILE_H

#include "fwk_base.h"

namespace fwk {

class Profiler {
  public:
	Profiler();
	~Profiler();

	static Profiler *instance();
	static double getTime();

	// TODO: Add hierarchical timers
	void updateTimer(const char *name, double start_time, double end_time, bool is_rare = true);
	void updateCounter(const char *name, int value);
	void nextFrame();
	const string getStats(const char *filter = "");

  private:
	struct Timer {
		Timer(const string &name) : name(name), last_frame_time(0.0), is_rare(false) {}

		string name;
		vector<pair<long long, double>> values;
		double last_frame_time;
		bool is_rare;
	};

	struct Counter {
		Counter(const string &name) : name(name), value(0) {}

		string name;
		long long value;
	};

	Timer &accessTimer(const string &name);
	Counter &accessCounter(const string &name);

	vector<Timer> m_timers;
	vector<Counter> m_counters;
	long long m_frame_count, m_frame_limit;
};

struct AutoTimer {
	AutoTimer(const char *id, bool is_rare)
		: start_time(Profiler::getTime()), id(id), is_rare(is_rare) {}
	~AutoTimer() {
		if(auto *profiler = Profiler::instance())
			profiler->updateTimer(id, start_time, Profiler::getTime(), is_rare);
	}

	double start_time;
	const char *id;
	bool is_rare;
};

#ifdef DISABLE_PROFILER

#define FWK_PROFILE(id)
#define FWK_PROFILE_RARE(id)
#define FWK_PROFILE_COUNTER(id, value)

#else

#define FWK_PROFILE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, false)
#define FWK_PROFILE_RARE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, true)

#define FWK_PROFILE_NAME(a) FWK_PROFILE_NAME_(a)
#define FWK_PROFILE_NAME_(a) timer##a

#define FWK_PROFILE_COUNTER(id, value)                                                             \
	{                                                                                              \
		if(auto *profiler = Profiler::instance())                                                  \
			profiler->updateCounter(id, value);                                                    \
	}

#endif
}

#endif
