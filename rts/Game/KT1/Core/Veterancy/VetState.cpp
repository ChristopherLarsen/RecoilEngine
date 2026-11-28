/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy core: VetState.h before step S26 (LUA-FREE H01): neutral stubs, no rule runs. No engine include.

#include "Game/KT1/Core/Veterancy/VetState.h"

namespace KT1::Core::Veterancy {

void VetState::Init(int32_t) {}

float Factor(const VetContext&, int32_t) { return 0; }
bool Threshold(const VetContext&, int32_t, int32_t, float&) { return false; }
float RegenAt(const VetContext&, int32_t, int32_t) { return 0; }
float ClassFactor(const VetContext&, VetClass) { return 0; }
VetMark Mark(const VetState&, const VetContext&, int32_t, int32_t) { return {}; }

void Credit(VetState&, const VetContext&, int32_t, float, VetWorld&) {}
void SetBase(VetState&, const VetContext&, int32_t, float, VetWorld&) {}
void SetCost(VetState&, int32_t, float) {}
void Carry(VetState&, int32_t, bool, float, int32_t) {}
void Rebuild(VetState&, const VetContext&, int32_t, VetWorld&) {}

void VetUnitDamaged(VetState&, const VetDamage&, VetWorld&) {}
void VetUnitDestroyed(VetState&, const VetContext&, const VetDeath&, int32_t, VetWorld&) {}
void CreditCargo(VetState&, const VetContext&, int32_t, int32_t, int32_t, int32_t, int32_t, VetWorld&) {}
void VetAllowTransfer(VetState&, int32_t, bool) {}
void VetUnitGiven(VetState&, const VetContext&, int32_t, VetWorld&) {}
void RunRegen(VetState&, const VetContext&, VetWorld&) {}

} // namespace KT1::Core::Veterancy
