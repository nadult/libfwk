// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "testing.h"

#include "fwk/filesystem.h"
#include "fwk/gfx/texture.h"
#include "fwk/sys/rollback.h"
#include "fwk/sys/stream.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <unistd.h>

struct Vec {
	int data[3];
	Vec(int x, int y, int z) : data{x, y, z} {}
	Vec() {}
	bool operator==(const Vec &v) const {
		return data[0] == v.data[0] && data[1] == v.data[1] && data[2] == v.data[2];
	}
};

SERIALIZE_AS_POD(Vec)

struct Obj0 {
	int a;
	Obj0() { a = 255; }
};
struct Obj1 {
	int b;
	Obj0 a;
	Obj1() { b = 16; }
};
struct Obj2 {
	int c;
	Obj1 a;
	Obj2() { c = 32; }
};

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

	Object() : v0(11.0f), v2(10.0f), flower(127) {}
	Object(int v0, float v1, long long v2, Vec v3, const char *text)
		: v0(v0), v1(v1), v2(v2), text(text), v3(v3), obj2(), flower(127) {}

	bool operator==(const Object &obj) const {
		return v0 == obj.v0 && v1 == obj.v1 && v2 == obj.v2 && v3 == obj.v3 && text == obj.text;
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

class TestStream : public FileStream {
  public:
	TestStream(const char *file_name, bool is_loading) : FileStream(file_name, is_loading) {}

	void v_load(void *data, int size) {
		FileStream::v_load(data, size);
		log("Reading data (%d): ", size);
		for(int n = 0; n < std::min(128, (int)size); n++)
			log("%x ", (unsigned)((unsigned char *)data)[n]);
		if(size > 128)
			log("...");
		log("\n");
	}

	void v_save(const void *data, int size) {
		log("Writing data (%d): ", size);
		for(int n = 0; n < std::min(128, (int)size); n++)
			log("%x ", (unsigned)((unsigned char *)data)[n]);
		if(size > 128)
			log("...");
		log("\n");
		FileStream::v_save(data, size);
	}

	TextFormatter log;
};

struct Object2 {
	void load(Stream &sr) { sr.signature("\0\0\0\0", 4); }
	void save(Stream &sr) const { sr.signature("\0\0\0\0", 4); }
};

struct Object3 {
	Vec tmp[5];
	Vec t2[30];
};

SERIALIZE_AS_POD(Object3)

void loadObj(Object &obj, Stream &sr) { sr >> obj; }
void saveObj(Object &obj, Stream &sr) { sr << obj; }

void loadObj(Object3 &obj, Stream &sr) { sr >> obj; }
void saveObj(Object3 &obj, Stream &sr) { sr << obj; }

string bigPerfTest() {
	Object object0(1, 2, 3, {4, 5, 6}, "dummy text");
	TextFormatter log;

	double time = getTime();
	log("Big performance test...\n");
	for(size_t n = 0; n < 10; n++) {
		{
			Saver svr("temp1.dat");
			for(size_t k = 0; k < 1000000; k++)
				saveObj(object0, svr);
		}
		Object object2;
		{
			Loader ldr("temp1.dat");
			for(size_t k = 0; k < 1000000; k++)
				loadObj(object2, ldr);
		}
	}
	log("Time: %f seconds\n\n", getTime() - time);
	return log.text();
}

void testPodData() {
	Object3 temp[3], temp1[3];
	{
		TestStream svr("temp2.dat", 0);
		svr << temp;
	}
	TestStream ldr("temp2.dat", 1);
	ldr >> temp1;

	if(memcmp(temp, temp1, sizeof(temp)) != 0)
		CHECK_FAILED("Error when serializing POD data");
}

void testFilesystem() {
	auto old_current = FilePath::current();
	auto data_dir = executablePath().parent().parent() / "data";
	ASSERT(access(data_dir / "test.blend"));
	FilePath::setCurrent(data_dir);
	ASSERT(FilePath::current() == data_dir.absolute());
	ASSERT(access("test.blend"));
	FilePath::setCurrent(old_current);
	ASSERT(FilePath::current() == old_current);
}

void testStreamRollback() {
	vector<char> tga_data;
	{
		auto data_path = FilePath(executablePath()).parent().parent() / "data";
		Loader ldr(data_path / "liberation_16_0.tga");
		tga_data.resize(ldr.size());
		ldr.loadData(tga_data.data(), tga_data.size());
	}

	Random rand;
	for(int n = 0; n < 100; n++) {
		vector<char> temp = tga_data;
		for(int i = 0; i < 4; i++)
			temp[rand.uniform(64)] = rand.uniform(256);

		{
			auto result = RollbackContext::begin(
				[&]() {
					MemoryLoader ldr(temp);
					return Texture(ldr, TextureFileType::tga);
				},
				BacktraceMode::disabled);
			if(result) {
				//	printf("Loaded properly\n");
			} else {
				//	printf("Failed\n");
				//	result.error().print();
			}
		}
	}
}

void testMain() {
	struct ScopeExit {
		~ScopeExit() {
			unlink("temp.dat");
			unlink("temp1.dat");
			unlink("temp2.dat");
		};
	} scope_exit;

	Object object0(1, 2, 3, {4, 5, 6}, "dummy text"), object1;
	Saver("temp.dat") << object0;
	Loader("temp.dat") >> object1;
	ASSERT(object0 == object1);

	Object2 object2;
	Loader ldr("temp.dat");
	ASSERT_FAIL(ldr >> object2;); // Wrong signature

	// TODO: disable printing backtraces on rollbacks (only if user wants to?)
	// TODO: fix this:
	// ASSERT(!ldr.allOk());

	ASSERT(SerializeAsPod<Vec>::value);
	ASSERT(!SerializeAsPod<string>::value);
	ASSERT(SerializeAsPod<Obj2>::value);
	ASSERT(!SerializeAsPod<Object>::value);
	ASSERT(SerializeAsPod<Vec[333]>::value);

	testPodData();

	vector<Vec> pods(1000), pods2;
	for(int n = 0; n < pods.size(); n++)
		pods[n] = Vec(n, n + 1, n + 2);
	Saver("temp.dat") << pods;
	Loader("temp.dat") >> pods2;
	ASSERT(pods2.size() == pods.size());
	for(int n = 0; n < pods.size(); n++)
		ASSERT(pods2[n] == pods[n]);

	testFilesystem();

	testStreamRollback();
}
