#include "fwk//format.h"
#include "fwk/math/rational.h"

namespace fwk {

void computeRationalApproximations() {
	Rational<int> br2, br3, br6;
	int best = 1;
	double best_err = 99999.0;

	// TODO: lepsze stałe
	static constexpr double sqrt_2 = 1.41421356237309504880;
	static constexpr double sqrt_3 = 1.73205080756887729352;
	static constexpr double sqrt_6 = sqrt_2 * sqrt_3; // TODO

	for(int d = 1; d < 1000000000; d++) {
		Rational<int> r2(double(d) * sqrt_2, d);
		Rational<int> r3(double(d) * sqrt_3, d);
		Rational<int> r6(double(d) * sqrt_6, d);
		if(double(r2) > sqrt_2)
			r2 = {r2.num() - 1, r2.den()};
		if(double(r3) > sqrt_3)
			r3 = {r3.num() - 1, r3.den()};
		if(double(r6) > sqrt_6)
			r6 = {r6.num() - 1, r6.den()};

		double err = fwk::abs(double(r2) - sqrt_2) / sqrt_2 +
					 fwk::abs(double(r3) - sqrt_3) / sqrt_3 +
					 fwk::abs(double(r6) - sqrt_6) / sqrt_6;
		if(err < best_err) {
			FWK_PRINT(r2, r3, r6, err * 1000000.0, d);
			br2 = r2;
			br3 = r3;
			br6 = r6;
			best_err = err;
			best = d;
		}
	}

	auto rat_sq2_down = br2;
	auto rat_sq3_down = br3;
	auto rat_sq6_down = br6;

	auto rat_sq2_up = Rational<int>{br2.num() + 1, br2.den()};
	auto rat_sq3_up = Rational<int>{br3.num() + 1, br3.den()};
	auto rat_sq6_up = Rational<int>{br6.num() + 1, br6.den()};
	FWK_PRINT(rat_sq2_down, double(rat_sq2_down) * 1000, rat_sq2_up, double(rat_sq2_up) * 1000);
	FWK_PRINT(rat_sq3_down, double(rat_sq3_down) * 1000, rat_sq3_up, double(rat_sq3_up) * 1000);
	FWK_PRINT(rat_sq6_down, double(rat_sq6_down) * 1000, rat_sq6_up, double(rat_sq6_up) * 1000);
}
}
