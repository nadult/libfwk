#include "fwk_math.h"
#include "fwk_base.h"
#include <SDL.h>

using namespace fwk;

bool almostEqual(const float3 &a, const float3 &b) { return distance(a, b) < constant::epsilon; }

void assertEqual(const float3 &a, const float3 &b) {
	if(!almostEqual(a, b))
		THROW("Error: (%f %f %f) != (%f %f %f)", a.x, a.y, a.z, b.x, b.y, b.z);
}

void testMatrices() {
	float3 up(0, 1, 0);
	float angle = constant::pi * 0.25f;

	float3 rot_a = mulNormal(rotation(up, angle), float3(1, 0, 0));
	float3 rot_b = mulNormal(rotation(up, angle), float3(0, 0, 1));

	for(int n = 0; n < 100; n++) {
		float3 trans(frand() * 100, frand() * 100, frand() * 100);
		float3 scale(1.0f + frand() * 2.0f, 1.0f + frand() * 2.0f, frand() * 2.0f + 1.0f);
		AxisAngle aa(
			normalize(float3(frand() * 2.0f - 1.0f, frand() * 2.0f - 1.0f, frand() * 2.0f - 1.0f)),
			frand() * constant::pi * 2.0f);

		Matrix4 mat = translation(trans) * scaling(scale) * Matrix4(Quat(aa));
		auto dec = decompose(mat);
		assertEqual(trans, dec.translation);
		assertEqual(scale, dec.scale);
	}

	// TODO: finish me
}

void testRays() {
	// TODO: write me
}

int main(int argc, char **argv) {
	try {
		FBox box(0, -100, 0, 1200, 100, 720);
		FBox temp(32, 0, 32, 64, 0.5f, 64);
		ASSERT(areOverlapping(box, temp));

		testMatrices();
		testRays();

		float3 vec(0, 0, 1);

		/*
		Quat rot = normalize(Quat::fromYawPitchRoll(0.5, 1.2, 0.3));
		Quat p = normalize(pow(rot, 1.0f));

		float3 v1 = mulNormal((Matrix4)rot, vec);
		float3 v2 = mulNormal((Matrix4)p, vec);

		printf("result:\n%f %f %f %f\n%f %f %f %f\n", rot.x, rot.y, rot.z, rot.w, p.x, p.y, p.z,
		p.w);
		printf("result:\n%f %f %f\n%f %f %f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);*/

		printf("All OK\n");
	} catch(const Exception &ex) {
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
	return 0;
}
