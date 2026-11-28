/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: the calls other native systems make into targeting, ORDERS' target pick first (LUA-FREE H01, COMBAT
// §4.4; step S19). They mirror GG.Cmb.Target, Engage, Engage2, BestKey, focus, InFlight, Contact, ScanFrames and
// ManualFireRefusal, and GG.CmdGun.
//
// Ids and values only; weapon numbers from 1; -1 is "no unit"; reaches in elmos. The per-step choice stays the fork's
// engine step (Lua/SupComTargeting.cpp): Target reads it. While the system is not live every call answers its neutral
// value and Active() is false: the caller then asks the Lua. The rule class of the system takes another name
// (TargetingRule): the namespace has this one.

#ifndef KT1_TARGETING_API_H
#define KT1_TARGETING_API_H

#include <cstdint>

#include "Game/KT1/Core/Targeting/TargetState.h"

namespace KT1::Targeting {

bool Active();   // the switch is on

int32_t Target(int32_t unit, int32_t weapon);   // the chosen target of one weapon; -1: none

// the reach and rank of the unit's weapon that takes the target; found false: none can, or the unit holds fire.
// noLine: the line of fire is not read. only: that weapon number alone, 0: all
Core::Targeting::Engagement Engage(int32_t unit, int32_t target, bool noLine, int32_t only);
Core::Targeting::Engagement2 Engage2(int32_t unit, int32_t target, bool noLine);   // weapon 1 and all, in one pass
// the best key Engage2 could give the unit on a def, from weapon kinds only; false: no weapon may take it
bool BestKey(int32_t unit, int32_t targetDef, int32_t& kind, int32_t& rank);

// the attack-move focus: every weapon that can take it and reaches it fires at it (GG.Cmb.focus[unit] = target)
int32_t Focus(int32_t unit);
void SetFocus(int32_t unit, int32_t target);    // -1: cleared

float InFlight(int32_t target);                 // damage of the shots flying at it
bool Contact(int32_t target, int32_t allyTeam); // REQ CMB-027 R2
// why a manual-fire order of the unit on the target is refused; None: it is not
Core::Targeting::Refusal ManualFireRefusal(int32_t unit, int32_t target);

int32_t ScanSteps(float intervalSeconds);       // REQ SIM-RATE-001 R2 (GG.Cmb.ScanFrames)
float ScanIntervalDefaultSeconds();
int32_t ScanExtraSteps();                       // the extra reference ticks of a scan, in steps (defences read it)

// the command unit's gun (GG.CmdGun)
int32_t CommandGunTarget(int32_t unit);         // -1: none
int32_t CommandGunNextScan(int32_t unit);       // -1: no command gun

} // namespace KT1::Targeting

#endif
