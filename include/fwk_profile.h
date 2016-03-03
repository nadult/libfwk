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
	static void openglFinish();

	// TODO: Add hierarchical timers
	void updateTimer(const char *name, double start_time, double end_time, bool is_rare = true);
	void updateCounter(const char *name, int value);
	void nextFrame(double expected_time = 1.0 / 60.0);
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

	Timer &accessTimer(const char *);
	Counter &accessCounter(const char *);

	vector<Timer> m_timers;
	vector<Counter> m_counters;
	long long m_frame_count, m_frame_limit;
	double m_last_frame_time;
};

struct AutoTimer {
	AutoTimer(const char *id, bool is_rare, bool is_opengl)
		: id(id), is_rare(is_rare), is_opengl(is_opengl) {
		if(is_opengl)
			Profiler::openglFinish();
		start_time = Profiler::getTime();
	}
	~AutoTimer() {
		if(is_opengl)
			Profiler::openglFinish();
		if(auto *profiler = Profiler::instance())
			profiler->updateTimer(id, start_time, Profiler::getTime(), is_rare);
	}

	double start_time;
	const char *id;
	bool is_rare, is_opengl;
};

#ifdef DISABLE_PROFILER

#define FWK_PROFILE(id)
#define FWK_PROFILE_RARE(id)
#define FWK_PROFILE_COUNTER(id, value)

#else

#define FWK_PROFILE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, false, false)
#define FWK_PROFILE_RARE(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, true, false)
#define FWK_PROFILE_OPENGL(id) fwk::AutoTimer FWK_PROFILE_NAME(__LINE__)(id, false, true)

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
