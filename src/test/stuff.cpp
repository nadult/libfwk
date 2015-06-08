/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"
#include <iostream>
#include <string>

using namespace std;
using namespace fwk;

int tmain() {
	try {
		THROW("SoMe RaNdOm TeXt %d %d %d %s %f", 11, 22, 33, " << ", 13.456);
	}
	catch(const Exception &ex) {
		cout << ex.what() << '\n' << ex.backtrace() << '\n';
	}

	//TODO: add tests for TextFormatter

	cout << "All OK\n";
	return 0;
}

int main(int argc, char **argv) {
	try {
		return tmain();
	}
	catch(const std::exception &ex) {
		cout << ex.what() << '\n';
	}
	catch(const char *str) {
		cout << str << '\n';
	}

	return 0;
}

