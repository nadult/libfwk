#include "fwk_math.h"
#include "fwk_base.h"
#include <SDL.h>

using namespace fwk;

bool almostEqual(const float3 &a, const float3 &b) {
	return distance(a, b) < constant::epsilon;
}

void testMatrices() {
	float3 up(0, 1, 0);
	float angle = constant::pi * 0.25f;

	float3 rot_a = mulNormal(rotation(up, angle), float3(1, 0, 0));
	float3 rot_b = mulNormal(rotation(up, angle), float3(0, 0, 1));

	//TODO: finish me
}

int main(int argc, char **argv) {
	FBox box(0, -100, 0, 1200, 100, 720);
	FBox temp(32, 0, 32, 64, 0.5f, 64);
	ASSERT(areOverlapping(box, temp));

	testMatrices();

	printf("All OK\n");
	return 0;
}
