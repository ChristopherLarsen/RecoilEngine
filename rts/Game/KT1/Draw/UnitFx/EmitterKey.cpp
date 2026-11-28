/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 UnitFx: the emitter key's rules. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/UnitFx/EmitterKey.h"

#include "Game/KT1/Draw/UnitFx/UnitFxConfig.h"

namespace KT1::UnitFx {

HealthBand HealthBandOf(const UnitFxConfig&, float, float, float) { return HealthBand::None; }
DetailBand DetailOf(const UnitFxConfig&, float, bool, bool, bool) { return DetailBand::None; }
float SizeFactor(const UnitFxConfig&, float) { return 0.0f; }

} // namespace KT1::UnitFx
