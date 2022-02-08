// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.
//
#pragma once

#include "fwk/sys_base.h"

namespace fwk {

class TestTimer {
  public:
	FWK_NO_INLINE TestTimer(const string &name) : name(name), start(fwk::getTime()) {}

	FWK_NO_INLINE ~TestTimer() {
		double now = getTime();
		printf("%s completed in %.4f msec\n", name.c_str(), (now - start) * 1000.0);
	}

  private:
	string name;
	double start;
};
}
