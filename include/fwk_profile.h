// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_PROFILE_H
#define FWK_PROFILE_H

#include "fwk/sys_base.h"
#include "fwk_vector.h"

namespace fwk {

namespace ProfileFlag {
	enum Type {
		opengl = 1,
		rare = 2,
	};
};

class Profiler {
  public:
	Profiler();
	~Profiler();

	static Profiler *instance() { return t_instance; }
	static double getTime();
	static void openglFinish();

	// TODO: Add hierarchical timers
	void updateTimer(const char *name, double start_time, double end_time, bool is_rare = true);
	void updateCounter(const char *name, int value);
	void nextFrame(double expected_time = 1.0 / 60.0);
	const string getStats(const char *filter = "");

  private:
	struct Timer {
		Timer(const string &name) : name(name) {}

		string name;
		vector<Pair<long long, double>> values;
		double last_frame_time = 0.0;
		double display_time = -1.0;
		bool is_rare = false;
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
	static __thread Profiler *t_instance;
};

struct ScopedProfile {
	ScopedProfile(const char *id, uint flags = 0, double min_time = 0.0);
	~ScopedProfile();

	double start_time, min_time;
	const char *id;
	uint flags;
};

#ifdef DISABLE_PROFILER

#define FWK_PROFILE(id)
#define FWK_PROFILE_RARE(id)
#define FWK_PROFILE_COUNTER(id, value)

#else

#define FWK_PROFILE(...) fwk::ScopedProfile FWK_PROFILE_NAME(__LINE__)(__VA_ARGS__)
#define FWK_PROFILE_RARE(id) fwk::ScopedProfile FWK_PROFILE_NAME(__LINE__)(id, ProfileFlag::rare)
#define FWK_PROFILE_OPENGL(id)                                                                     \
	fwk::ScopedProfile FWK_PROFILE_NAME(__LINE__)(id, ProfileFlag::opengl)

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
