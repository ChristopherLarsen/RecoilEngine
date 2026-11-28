/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxBuild: the criteria effects and their shake. Stub (LUA-FREE H01): step D15 writes the bodies.

#include "Game/KT1/Draw/FxBuild/CriteriaFx.h"

namespace KT1::FxBuild {

DeathBand BandOfDeath(const CriteriaConfig&, float, Core::Fx::DeathMedium) { return DeathBand::Small; }
void DebrisRange(const CriteriaConfig&, float, int32_t& least, int32_t& most) { least = 0; most = 0; }
void OnHit(CriteriaState&, const CriteriaConfig&, const ShakeConfig&, const Core::Fx::Vec3&, const Core::Fx::WeaponFx*, bool,
	float) {}
void OnDeath(CriteriaState&, const CriteriaConfig&, const Core::Fx::CriteriaTimingConfig&, const Core::Fx::CritDeath&,
	const Core::Fx::UnitDefFx&, float) {}
void StepCriteria(CriteriaState&, const CriteriaConfig&, const ShakeConfig&, float, uint32_t, std::vector<DeathMarkAsk>&) {}
float ShakeAmp(const ShakeConfig&, const Shake&, float, float) { return 0.0f; }
bool CameraOffset(ShakeState&, const ShakeConfig&, const Core::Fx::Vec3&, float, Core::Fx::Vec3&) { return false; }

} // namespace KT1::FxBuild
