/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxDecals: the terrain dressing. Stub (LUA-FREE H01): step D16 writes the bodies.

#include "Game/KT1/Draw/Decals/Dressing.h"

namespace KT1::FxDecals {

void SeedDressing(WichmannHill&, const DressingIn&, const DressingWorld&) {}
float NextUnit(WichmannHill&) { return 0.0f; }
float CutoffElmos(const DressingConfig&, float) { return 0.0f; }
float CutoffQuantile(float, float) { return 0.0f; }
int32_t DressingCount(const DressingConfig&, const DressingWorld&, float) { return 0; }
void PlaceDressing(DressingState&, const DressingConfig&, const DressingIn&, const DressingWorld&, DecalOutput&) {}
void ApplyZoom(DressingState&, float, float, DecalOutput&) {}

} // namespace KT1::FxDecals
