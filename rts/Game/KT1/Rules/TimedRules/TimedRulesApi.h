/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules: what other native systems ask the timed rows (LUA-FREE H01, step S21): the unit scripts' charge gate
// (GG.Charge), stun readers (GG.Stun), the economy's answer, and the rows and effects later families add.
//
// Ids and values only; weapon numbers from 1. While the system is not live every call answers its neutral value and
// Active() is false: the caller then asks the Lua. The rule class of the system takes another name (TimedRulesRule).

#ifndef KT1_TIMED_RULES_API_H
#define KT1_TIMED_RULES_API_H

#include <cstdint>
#include <string_view>

#include "Game/KT1/Core/TimedRules/TimedState.h"

namespace KT1::TimedRules {

bool Active();   // the switch is on

// GG.Charge.Fire: true when the shot may fire (a complete charge, which it uses). Not live: false.
bool ChargeFire(int32_t unit, int32_t weapon);
void ChargeWind(int32_t unit, int32_t weapon, float progress);   // the wind-up's look (REQ CMB-001 R3)
void ChargeGrant(int32_t unit, int32_t weapon, float want, float share);   // the economy's answer to a request

bool Stunned(int32_t unit);
void SetStunEnabled(int32_t unit, bool on);   // REQ CMB-023 R6: the unit's enabling upgrade

// rows of data/kt1/timedrules.json. A system that adds an effect registers it at game setup, then arms rows on units;
// the effect is called at the start and at the end.
int32_t RuleIndex(std::string_view id);       // look up once, -1: no such row
void SetEffect(std::string_view name, Core::TimedRules::EffectFn fn, void* context);
bool Arm(int32_t rule, int32_t unit, int32_t durationSteps);   // the later end stands
void Disarm(int32_t rule, int32_t unit);
int32_t DueStep(int32_t rule, int32_t unit);  // -1: not active

} // namespace KT1::TimedRules

#endif
