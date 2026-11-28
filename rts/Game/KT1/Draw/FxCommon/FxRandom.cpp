/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 EFFECTS draw: the seeded generator. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/FxCommon/FxRandom.h"

namespace KT1::FxDraw {

void Random::Seed(uint32_t) {}
void Random::SeedFrom(uint32_t, uint32_t, uint32_t) {}
float Random::Unit() { return 0.0f; }
float Random::Range(float, float) { return 0.0f; }
float Random::Signed() { return 0.0f; }
int32_t Random::Int(int32_t, int32_t) { return 0; }

} // namespace KT1::FxDraw
