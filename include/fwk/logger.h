// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum.h"
#include "fwk/hash_map.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace fwk {

class Logger {
  public:
	Logger();
	FWK_COPYABLE_CLASS(Logger);

	void addMessage(Str, Str unique_key);
	bool keyPresent(Str) const;

  private:
	HashMap<string, int> m_keys;
};
}
