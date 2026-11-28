/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: unit death and damage-state sets. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxDeath.h"

namespace KT1::Core::Fx {

bool ResolveDeathPlan(const DeathConfig&, DeathPlan&) { return false; }
DeathLayer LayerOf(const DeathConfig&, float, float) { return DeathLayer::Land; }
int32_t DamageClassOf(DeathKind) { return 0; }
DeathParts PartsOfDeath(const DeathConfig&, const DeathPlan&, DeathKind, DeathLayer, float, float, float,
	int32_t (*)(void*, int32_t, int32_t), void*) { return {}; }
uint32_t ActiveStates(const DeathPlan&, int32_t, float) { return 0; }
int32_t DamageEmitters(const DeathConfig&, float) { return 0; }

} // namespace KT1::Core::Fx
