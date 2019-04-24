// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert_info.h"

namespace fwk {

string AssertInfo::preFormat(TextFormatter &out, const char *prefix) const {
	const char *fmt;
	string temp;

	if(strlen(arg_names) > 0) {
		out("%%\n", prefix, message);
		return detail::autoPrintFormat(arg_names);
	} else {
		out << prefix;
		return message;
	}
}
}
