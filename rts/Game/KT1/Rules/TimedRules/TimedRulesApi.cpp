/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules: TimedRulesApi.h before step S21 (LUA-FREE H01): the system is not live, every answer is neutral.

#include "TimedRulesApi.h"

namespace KT1::TimedRules {

bool Active() { return false; }

bool ChargeFire(int32_t, int32_t) { return false; }
void ChargeWind(int32_t, int32_t, float) {}
void ChargeGrant(int32_t, int32_t, float, float) {}

bool Stunned(int32_t) { return false; }
void SetStunEnabled(int32_t, bool) {}

int32_t RuleIndex(std::string_view) { return -1; }
void SetEffect(std::string_view, Core::TimedRules::EffectFn, void*) {}
bool Arm(int32_t, int32_t, int32_t) { return false; }
void Disarm(int32_t, int32_t) {}
int32_t DueStep(int32_t, int32_t) { return -1; }

} // namespace KT1::TimedRules
