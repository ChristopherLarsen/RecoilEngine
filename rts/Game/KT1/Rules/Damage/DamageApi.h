/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage: what other native systems ask the damage system (LUA-FREE H01, COMBAT §4.2; step S21). It gives what
// GG.Cmb.lastKill, GG.CmbPatterns, GG.SpcExp and GG.RemoveWithoutBlast give the Lua today. The shots in flight at a
// target are targeting's (TargetingApi.h InFlight).
//
// Ids and values only. While the system is not live (off or check) every call answers its neutral value and Active() is
// false: the caller then asks the Lua. The rule class of the system takes another name (DamageRule): the namespace has
// this one.

#ifndef KT1_DAMAGE_API_H
#define KT1_DAMAGE_API_H

#include <cstdint>

#include "Game/KT1/Core/Damage/BlastState.h"

namespace KT1::Damage {

bool Active();   // the switch is on: the answers below are the native ones

// REQ CMB-008 R3: the unit's killing hit; false: none recorded
bool LastKill(int32_t unit, Core::Damage::KillRecord& out);

// GG.RemoveWithoutBlast[unit] = true: the unit is being removed, not killed (REQ CMB-008 R4)
void MarkNoBlast(int32_t unit);
bool NoBlastMarked(int32_t unit);

// falling aircraft (GG.SpcExp)
int32_t CrashingSince(int32_t unit);   // the step of the killing hit; -1: not crashing
bool CrashKillOf(int32_t unit, Core::Damage::CrashKill& out);
bool CrashContactOf(int32_t unit, Core::Damage::CrashContact& out);
const Core::Damage::BlastDef* EffectOf(int32_t unitDef);   // nullptr: the def has no death effect

// REQ CMB-022: what the Explosion call-in does for a pattern weapon (GG.CmbPatterns.Impact); false: no pattern
bool Impact(int32_t weaponDef, const float (&pos)[3], int32_t attacker);
int32_t Splits();                      // torpedoes that split

// REQ CMB-007: the multiplier of a damage type ("" or nullptr: the default type) against a unit def's armour
float ArmourMult(int32_t unitDef, const char* damageType);

// the combat values other systems read from cmb_values.lua (defences, wrecks, death animation)
float DeathThreshold();                // REQ CMB-008 R2
float DeathDelaySeconds();             // a fixture's delay for every death; -1: unset
float LandDeathToWreckSeconds();
float ShipDeathToWreckSeconds();       // REQ ECO-009 R6

} // namespace KT1::Damage

#endif
