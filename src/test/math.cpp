#include "fwk_math.h"
#include "fwk_base.h"

using namespace fwk;

int main() {
	FBox box(0, -100, 0, 1200, 100, 720);
	FBox temp(32, 0, 32, 64, 0.5f, 64);

	ASSERT(areOverlapping(box, temp));
	printf("All OK\n");
	return 0;
}
