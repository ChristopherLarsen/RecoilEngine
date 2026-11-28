/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: BlastState.h before step S21 (LUA-FREE H01): neutral stubs, no rule runs. No engine include.

#include "Game/KT1/Core/Damage/BlastState.h"

namespace KT1::Core::Damage {

const char* BlastKindName(BlastKind) { return ""; }

BlastDef MakeBlastDef(const BlastDefSource&, const DamageConfig&, const ArmourTable&) { return {}; }

BlastParams MakeBlastParams(const DamageConfig&, int32_t, int32_t) { return {}; }

void BlastState::Init(int32_t) {}

void BlastUnitCreated(BlastState&, int32_t) {}
void MarkNoBlast(BlastState&, int32_t) {}
void BlastUnitDamaged(BlastState&, const BlastContext&, const UnitDamage&, int32_t, BlastWorld&) {}
void BlastUnitDestroyed(BlastState&, const BlastContext&, const UnitDeath&, int32_t, BlastWorld&) {}
void RunBlasts(BlastState&, const BlastContext&, int32_t, BlastWorld&) {}

} // namespace KT1::Core::Damage
