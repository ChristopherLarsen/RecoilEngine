/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxLights: the flash rules. Stub (LUA-FREE H01): step D04 writes the bodies.

#include "Game/KT1/Draw/FxLights/LightList.h"

namespace KT1::FxLights {

Flash FlashOf(const LightsConfig&, const Core::Fx::WeaponFx&, float) { return {}; }
float Envelope(const LightsConfig&, float) { return 0.0f; }
Vec3 MuzzleOf(const Vec3&, const Vec3&) { return {}; }
bool JustFired(const LightsConfig&, const Vec3&, const Vec3&, const Vec3&, float) { return false; }
int32_t PlanFlashes(const LightsConfig&, const LightList&, std::span<const ShotLight>, int32_t, const Vec3&,
	std::vector<PlannedLight>&) { return 0; }
void InitLights(LightList&, const LightsConfig&, int32_t) {}
void AddFlashes(LightList&, const LightsConfig&, std::span<const PlannedLight>, float) {}
void ExpireLights(LightList&, float) {}

} // namespace KT1::FxLights
