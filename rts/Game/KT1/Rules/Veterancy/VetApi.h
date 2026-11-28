/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy: what other native systems ask veterancy (LUA-FREE H01, COMBAT §4.7; step S26). It gives what GG.Vet
// gives death_blast, game_eco and ord_cargo_death today. Draw code reads Snapshot<Core::Veterancy::VetMark> instead.
//
// Ids and values only. While the system is not live every call answers its neutral value and Active() is false: the
// caller then asks the Lua (the mod option veterancy "0" keeps it not live and there is no Lua either). The rule class
// of the system takes another name (VeterancyRule): the namespace has this one.

#ifndef KT1_VET_API_H
#define KT1_VET_API_H

#include <cstdint>

#include "Game/KT1/Core/Veterancy/VetState.h"

namespace KT1::Veterancy {

bool Active();   // the switch is on

int32_t Level(int32_t unit);
bool Xp(int32_t unit, float& out);                  // false: the unit has no experience record
bool Mark(int32_t unit, Core::Veterancy::VetMark& out);
bool Bounds(int32_t unit, float& prev, float& next);

void Credit(int32_t unit, float mass);              // REQ VET-001 R1, VET-002 R1
// REQ ORD-014 R5: before a falling transport's cargo goes
void CreditCargo(int32_t transport, int32_t team, int32_t attacker, int32_t attackerTeam);
// a capture that makes a new unit (REQ VET-003 R4): copy the old unit's record, then the new unit's base maximum
void Carry(int32_t fromUnit, int32_t toUnit);
void SetBase(int32_t unit, float maxHealth);
void SetCost(int32_t unit, float mass);

} // namespace KT1::Veterancy

#endif
