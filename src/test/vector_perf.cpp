#include <cstdlib>

#include <iostream>
#include <string>

#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#include "fwk_base.h"
#include "fwk_math.h"

class TestTimer {
  public:
	TestTimer(const std::string &name)
		: name(name),
		  start(boost::date_time::microsec_clock<boost::posix_time::ptime>::local_time()) {}

	~TestTimer() {
		using namespace std;
		using namespace boost;

		posix_time::ptime now(date_time::microsec_clock<posix_time::ptime>::local_time());
		posix_time::time_duration d = now - start;

		cout << name << " completed in " << d.total_milliseconds() / 1000.0 << " seconds" << endl;
	}

  private:
	std::string name;
	boost::posix_time::ptime start;
};

struct Pixel {
	Pixel() {}

	Pixel(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}

	unsigned char r, g, b;
};

template <template <typename> class T> void testVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 1000; ++i) {
		int dimension = 500;

		T<Pixel> pixels;
		pixels.resize(dimension * dimension);

		for(int i = 0; i < dimension * dimension; ++i) {
			pixels[i].r = 255;
			pixels[i].g = 0;
			pixels[i].b = 0;
		}
	}
}

template <template <typename> class T> void testVectorPushBack(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 1000; ++i) {
		int dimension = 500;

		T<Pixel> pixels;
		pixels.reserve(dimension * dimension);

		for(int i = 0; i < dimension * dimension; ++i)
			pixels.push_back(Pixel(255, 0, 0));
	}
}

template <template <typename> class T> void testVectorVector(const char *name) {
	TestTimer t(name);

	for(int i = 0; i < 100; ++i) {
		int dimension = 200;

		T<T<fwk::int3>> temp;
		for(int i = 0; i < dimension * dimension; ++i) {
			T<fwk::int3> tout;
			tout.reserve(8);

			for(int axis = 0; axis < 3; axis++) {
				fwk::int3 npos(1, 2, 3);
				npos[axis] += 1;
				tout.emplace_back(npos);
				npos[axis] -= 2;
				tout.emplace_back(npos);
			}
			temp.emplace_back(tout);
		}
	}
}

template <class T> using stdvec = std::vector<T, fwk::SimpleAllocator<T>>;

int main() {
	testVector<fwk::vector>("fwk::vector simple");
	testVector<stdvec>("std::vector simple");
	testVectorPushBack<fwk::vector>("fwk::vector push_back");
	testVectorPushBack<stdvec>("std::vector push_back");
	testVectorVector<fwk::vector>("fwk::vector vector^2");
	testVectorVector<stdvec>("std::vector vector^2");
	return 0;
}
