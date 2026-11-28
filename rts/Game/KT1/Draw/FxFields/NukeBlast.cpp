/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the large blast's player. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/NukeBlast.h"

namespace KT1::FxFields {

uint32_t NukeSeed(float, float, int32_t) { return 0; }
NukeWave WaveOf(float, int32_t, float, float) { return {}; }
void PlanNuke(const NukeConfig&, std::string_view, const NukeState&, NukePlan&) {}
NukeEmitted EmitNuke(const NukeConfig&, const NukePlan&, const Core::Fx::Vec3&, float, uint32_t, NukePools&) { return {}; }

} // namespace KT1::FxFields
