/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitFx: the API of the unit effects. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/UnitFx/UnitFxApi.h"

namespace KT1::UnitFx {

bool Active() { return false; }
void Foot(int32_t, float, float, float, Core::Fx::SizeBucket, int32_t, int32_t) {}
void Blast(Core::Fx::BlastClass, float, float, float, int32_t) {}
int32_t HealthBandOfUnit(int32_t) { return 0; }
uint32_t Particles() { return 0; }

void PushUnitFxBridge(lua_State*) {}

} // namespace KT1::UnitFx
