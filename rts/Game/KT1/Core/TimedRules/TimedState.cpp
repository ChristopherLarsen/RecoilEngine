/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules core: TimedState.h before step S21 (LUA-FREE H01): neutral stubs, no rule runs. No engine include.

#include "Game/KT1/Core/TimedRules/TimedState.h"

namespace KT1::Core::TimedRules {

TimedParams MakeTimedParams(const TimedRulesConfig&, int32_t) { return {}; }

void TimedState::Init(int32_t, const TimedRulesConfig&) {}

void TimedUnitCreated(TimedState&, const TimedContext&, int32_t, int32_t) {}
void TimedUnitDestroyed(TimedState&, int32_t) {}

void StunExplosion(TimedState&, const TimedContext&, int32_t, const float (&)[3], int32_t, int32_t, TimedWorld&) {}
bool Stun(TimedState&, const TimedContext&, int32_t, const StunEntry&, int32_t, int32_t, TimedWorld&) { return false; }
bool Stunned(const TimedState&, int32_t) { return false; }
void SetStunEnabled(TimedState&, int32_t, bool, TimedWorld&) {}

bool ChargeFire(TimedState&, const TimedContext&, int32_t, int32_t, int32_t, TimedWorld&) { return false; }
void ChargeWind(TimedState&, const TimedContext&, int32_t, int32_t, float, int32_t, TimedWorld&) {}
void ChargeSet(TimedState&, const TimedContext&, int32_t, int32_t, float, TimedWorld&) {}
void ChargeGrant(TimedState&, const TimedContext&, int32_t, int32_t, float, float) {}

void RunCharges(TimedState&, const TimedContext&, int32_t, TimedWorld&) {}
void RunCrush(TimedState&, const TimedContext&, int32_t, TimedWorld&) {}
void RunDue(TimedState&, const TimedContext&, int32_t, TimedWorld&) {}

int32_t RuleIndex(const TimedState&, std::string_view) { return NONE; }
void SetEffect(TimedState&, std::string_view, EffectFn, void*) {}
bool Arm(TimedState&, int32_t, int32_t, int32_t) { return false; }
void Disarm(TimedState&, int32_t, int32_t) {}
int32_t DueStep(const TimedState&, int32_t, int32_t) { return NONE; }

} // namespace KT1::Core::TimedRules
