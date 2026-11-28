/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: DamageState.h before step S21 (LUA-FREE H01): neutral stubs, no rule runs. No engine include.

#include "Game/KT1/Core/Damage/DamageState.h"

namespace KT1::Core::Damage {

DamageParams MakeParams(const DamageConfig&, int32_t) { return {}; }

void ArmourTable::Build(const DamageConfig&) {}
int32_t ArmourTable::ClassIndex(std::string_view) const { return NONE; }
int32_t ArmourTable::TypeIndex(std::string_view) const { return NONE; }
float ArmourTable::Mult(int32_t, int32_t) const { return 1; }

void DamageState::Init(int32_t) {}

HitResult DamageRules(DamageState&, const DamageDefs&, const DamageParams&, const Hit& hit, int32_t, World&) {
	return {hit.damage, 1, hit.paralyzer, RULE_NONE};
}

HitResult PatternRules(DamageState&, const DamageDefs&, const DamageParams&, const Hit&, const HitResult& in) { return in; }

int32_t SplashExplosion(DamageState&, const DamageDefs&, const DamageParams&, int32_t, const float (&)[3], int32_t, World&) {
	return 0;
}

bool PatternImpact(DamageState&, const DamageDefs&, const DamageParams&, int32_t, const float (&)[3], int32_t, int32_t) {
	return false;
}

void ProjectileCreated(DamageState&, const DamageDefs&, int32_t, int32_t, int32_t) {}
void ProjectileDestroyed(DamageState&, int32_t) {}

void RunDotPulses(DamageState&, const DamageDefs&, int32_t, World&) {}
void RunSplits(DamageState&, const DamageDefs&, const DamageParams&, int32_t, World&) {}
void RunPatternPulses(DamageState&, const DamageDefs&, int32_t, World&) {}

void SplitWeave(const DamageParams&, const WeaponRules&, float, float& amplitudeMu, float& frequency) {
	amplitudeMu = 0;
	frequency = 0;
}

} // namespace KT1::Core::Damage
