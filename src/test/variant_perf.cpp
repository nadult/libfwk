#include <cstdlib>

#include <iostream>
#include <string>

#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/variant.hpp>

#include "fwk_base.h"
#include "fwk_math.h"
#include "fwk_variant.h"
#include "fwk_xml.h"

class TestTimer {
  public:
	TestTimer(const std::string &name) __attribute__((noinline))
	: name(name), start(boost::date_time::microsec_clock<boost::posix_time::ptime>::local_time()) {}

	~TestTimer() __attribute__((noinline)) {
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

class SizeVisitor : public boost::static_visitor<> {
  public:
	int size = 0;
	template <class T> void operator()(const T &rhs) { size = sizeof(rhs); }
};

template <class Visitor, typename... Types>
auto visit(boost::variant<Types...> &variant, Visitor &visitor) {
	boost::apply_visitor(visitor, variant);
}

template <class Visitor, typename... Types>
auto visit(fwk::Variant<Types...> &variant, Visitor &visitor) {
	fwk::apply_visitor(visitor, variant);
}

DEFINE_ENUM(enum1, aa, bb, cc);
DEFINE_ENUM(enum2, qq, rr, tt);

template <template <typename...> class T> void testVariantSimple(const char *name) {
	TestTimer t(name);
	using namespace fwk;

	using Type = T<enum1, enum2, short>;
	int iters = 10000000;

	vector<Type> values;
	values.reserve(iters);

	srand(0);
	for(int n = 0; n < iters; n++) {
		Type variant;
		int t = rand();
		if(t % 3 == 0)
			variant = (enum1)(t / 128);
		else if(t % 3 == 1) {
			variant = (enum2)(t / 128);
		} else
			variant = (short)(t / 64);
		values.emplace_back(variant);
	}

	int sum = 0;
	for(auto &v : values) {
		SizeVisitor visitor;
		visit(v, visitor);
		sum += visitor.size;
	}

	xmlPrint("Result: % Size: %\n", sum, sizeof(Type));
}

template <template <typename...> class T> void testVariantBigger(const char *name) {
	TestTimer t(name);
	using namespace fwk;

	using Type = T<Matrix4, IRect, string>;
	int iters = 3000000;

	vector<Type> values;
	values.reserve(iters);

	srand(0);
	for(int n = 0; n < iters; n++) {
		Type variant;
		int t = rand();
		if(t % 3 == 0)
			variant = Matrix4::identity();
		else if(t % 3 == 1) {
			variant = IRect(10, 10, 20, 20);
		} else
			variant = "Hello world";
		values.emplace_back(variant);
	}

	int sum = 0;
	for(auto &v : values) {
		SizeVisitor visitor;
		visit(v, visitor);
		sum += visitor.size;
	}

	xmlPrint("Result: % Size: %\n", sum, sizeof(Type));
}

int main() {
	testVariantSimple<boost::variant>("boost::variant simple");
	testVariantSimple<fwk::Variant>("fwk::Variant   simple");
	testVariantBigger<boost::variant>("boost::variant bigger");
	testVariantBigger<fwk::Variant>("fwk::Variant   bigger");

	return 0;
}