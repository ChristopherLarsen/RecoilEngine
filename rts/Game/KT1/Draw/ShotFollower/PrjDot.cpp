/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 ShotFollower: the projectile dot rules. Stub (LUA-FREE H01): step D05 writes the bodies.

#include "Game/KT1/Draw/ShotFollower/PrjDot.h"

namespace KT1::ShotFollower {

bool PrjDotOn(const PrjDotConfig&, float) { return false; }
float PrjDotZoom(const PrjDotConfig&, const float (&)[3], const float (&)[3]) { return 0.0f; }
void PrjDotColour(const PrjDotConfig&, const float*, float (&out)[4]) {
	for (float& v : out) v = 0.0f;
}
float PrjDotPixels(const PrjDotConfig&, float, std::string_view) { return 0.0f; }
float PrjIconPixels(const PrjDotConfig&, float) { return 0.0f; }
float PrjDotGlow(const PrjDotConfig&, float) { return 0.0f; }
const PrjDotIcon* PrjDotIconOf(const PrjDotConfig&, std::string_view) { return nullptr; }

} // namespace KT1::ShotFollower
