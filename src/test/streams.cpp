/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <SDL.h>

using namespace fwk;
using std::cout;

struct Vec {
	int data[3];
	Vec(int x, int y, int z) :data{x, y, z} { }
	Vec() { }
	bool operator==(const Vec &v) const {
		return data[0] == v.data[0] && data[1] == v.data[1] && data[2] == v.data[2];
	}
};

SERIALIZE_AS_POD(Vec)

struct Obj0 { int a; Obj0() { a=255; } };
struct Obj1 { int b; Obj0 a; Obj1() { b = 16; } };
struct Obj2 { int c; Obj1 a; Obj2() { c = 32; } };

SERIALIZE_AS_POD(Obj0)
SERIALIZE_AS_POD(Obj1)
SERIALIZE_AS_POD(Obj2)

struct Object {
	long long v0;
	long long v1;
	float v2;
	string text;
	Vec v3;
	Obj2 obj2;
	char flower;

	Object() :v0(v2),flower(127) { }
	Object(int v0, float v1, long long v2, Vec v3, const char *text)
		:v0(v0), v1(v1), v2(v2), text(text), v3(v3), obj2(), flower(127) { }

	bool operator==(const Object &obj) const {
		return v0 == obj.v0 && v1 == obj.v1 && v2 == obj.v2 && v3 == obj.v3 &&
			text == obj.text;
	}


	void load(Stream &sr) {
		sr.unpack(flower, v0, v1, v2, v3, obj2);
		sr >> text;
	}
	void save(Stream &sr) const {
		sr.pack(flower, v0, v1, v2, v3, obj2);
		sr << text;
	}
};

class TestStream: public FileStream
{
public:
	TestStream(const char *file_name, bool is_loading)
		:FileStream(file_name, is_loading) { }

	void v_load(void *data, int size) {
		FileStream::v_load(data, size);
		try {
			cout << "Reading data (" << size << "): ";
			for(int n = 0; n < std::min(128, (int)size); n++)
				cout << std::hex << (unsigned)((unsigned char*) data)[n] << ' ';
			if(size > 128) cout << "...";
			cout << std::dec << '\n';
		} catch(...) { }
	}

	void v_save(const void *data, int size) {
		try {
			cout << "Writing data (" << size << "): ";
			for(int n = 0; n < std::min(128, (int)size); n++)
				cout << std::hex << (unsigned)((unsigned char*) data)[n] << ' ';
			if(size > 128) cout << "...";
			cout << std::dec << '\n';
		} catch(...) { }
		FileStream::v_save(data, size);
	}
};

struct Object2 {
	void load(Stream &sr) {
		sr.signature("\0\0\0\0", 4);
	}
	void save(Stream &sr) const {
		sr.signature("\0\0\0\0", 4);
	}
};

struct Object3 {
	Vec tmp[5];
	Vec t2[30];
};

SERIALIZE_AS_POD(Object3)

void loadObj(Object &obj, Stream &sr) __attribute__((noinline));
void loadObj(Object &obj, Stream &sr) { sr >> obj; }
void saveObj(Object &obj, Stream &sr) __attribute__((noinline));
void saveObj(Object &obj, Stream &sr) { sr << obj; }

void loadObj(Object3 &obj, Stream &sr) __attribute__((noinline));
void loadObj(Object3 &obj, Stream &sr) { sr >> obj; }
void saveObj(Object3 &obj, Stream &sr) __attribute__((noinline));
void saveObj(Object3 &obj, Stream &sr) { sr << obj; }

void testIntEncoding() {
		int min = -2ll * 1024 * 1024 * 1024, max = 2ll * 1024 * 1024 * 1024 - 1, step = 1024 * 1024 * 32;

		printf("Testing int encoding: \n");

#pragma omp parallel for
		for(int i = min / step; i < max / step; i++) {
			vector<char> buffer(1024 * 1024 * 200);
			MemorySaver sstream(buffer.data(), buffer.size());

			int end = (long long)max - i * step < step? max : i * step + step;
			for(int j = i * step; j < end; j++)
				sstream.encodeInt(j);

			MemoryLoader lstream(buffer.data(), sstream.pos());
			for(int j = i * step; j < end; j++)
				ASSERT(lstream.decodeInt() == j);

			printf("%11d - %11d: OK\n", i * step, end);
		}
}

int tmain() {
	Object object0(1, 2, 3, {4,5,6}, "dummy text"), object1;

	Saver("temp.dat") << object0;
	Loader("temp.dat") >> object1;
	ASSERT(object0 == object1); 

	ASSERT(SerializeAsPod<Vec>::value);
	ASSERT(!SerializeAsPod<string>::value);
	ASSERT(SerializeAsPod<Obj2>::value);
	ASSERT(!SerializeAsPod<Object>::value);
	ASSERT(SerializeAsPod<Vec[333]>::value);

	{
		cout << "Serializing big object as a POD\n";
		Object3 temp[3], temp1[3];
		{
			TestStream svr("temp2.dat", 0);
			svr << temp;
		}
		TestStream ldr("temp2.dat", 1);
		ldr >> temp1;
		cout << "\n\n";

		ASSERT(memcmp(temp, temp1, sizeof(temp)) == 0);
	}
	{
		{
			TestStream svr("temp1.dat", 0);
			saveObj(object0, svr);
		}
		Object object2; {
			TestStream ldr("temp1.dat", 1);
			loadObj(object2, ldr);
		}
		ASSERT(object2 == object0);
		cout << "\n";
	}

	{
		double time = getTime();
		cout << "Big performance test...\n";
		for(size_t n = 0; n < 10; n++) {
			{
				Saver svr("temp1.dat");
				for(size_t k = 0; k < 1000000; k++) saveObj(object0, svr);
			}
			Object object2; {
				Loader ldr("temp1.dat");
				for(size_t k = 0; k < 1000000; k++) loadObj(object2, ldr);
			}
		}
		cout << "Time: " << getTime() - time << " seconds\n\n";
	}

	{
		Object2 object2;
		Loader ldr("temp.dat");

		try {
			ldr >> object2;
			throw "Exception should be thrown...";
		}
		catch(const Exception &ex) {
			cout << "Wrong signature exception:\n" << ex.what() << "\n\n";
		}
		ASSERT(!ldr.allOk());
	}

	try {
		ASSERT(!"Thats how failed assertion will look like");
	}
	catch(const Exception &ex) {
		cout << "Assertion testing:\n" << ex.what()
			<< "\nBacktrace:\n" << cppFilterBacktrace(ex.backtrace()) << "\n\n";
	}

	{
		vector<Vec> pods(1000), pods2;
		for(size_t n = 0; n < pods.size(); n++) pods[n] = Vec(n, n+1, n+2);
		Saver("temp.dat") << pods;
		Loader("temp.dat") >> pods2;
		ASSERT(pods2.size() == pods.size());
		for(size_t n = 0; n < pods.size(); n++)
			ASSERT(pods2[n] == pods[n]);
	}

	unlink("temp.dat");
	unlink("temp1.dat");
	unlink("temp2.dat");

	testIntEncoding();

	cout << "All OK\n";
	return 0;
}

int main(int argc, char **argv) {
	try {
		return tmain();
	}
	catch(const Exception &ex) {
			cout << ex.what() << "\nBacktrace:\n" << cppFilterBacktrace(ex.backtrace()) << "\n";
	}
	catch(const std::exception &ex) {
		cout << ex.what() << '\n';
	}
	catch(const char *str) {
		cout << str << '\n';
	}

	return 0;
}

