// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.
//
#pragma once

#include "fwk/sys_base.h"

namespace fwk {

class TestTimer {
  public:
	TestTimer(const string &name) NOINLINE : name(name), start(fwk::getTime()) {}

	~TestTimer() NOINLINE {
		double now = getTime();
		printf("%s completed in %.4f seconds\n", name.c_str(), (now - start));
	}

  private:
	string name;
	double start;
};
}
