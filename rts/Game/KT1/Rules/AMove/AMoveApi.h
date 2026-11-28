/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove: what systems Orders and OrderRules call. Other families ask Rules/Orders/OrdersApi.h. With system AMove off
// every read answers "nothing" and every call does nothing. System AMove registers its own phase (OrdersInternal.h
// PHASE_AMOVE) and hears UnitDestroyed itself only for what these calls do not cover.

#ifndef KT1_RULES_AMOVE_API_H
#define KT1_RULES_AMOVE_API_H

#include <cstdint>

#include "Game/KT1/Core/AMove/AMoveState.h"

namespace KT1 {

struct SimContext;

namespace AMove {

// nullptr while the system is off
const Core::AMove::State* State();

// the attack-move order reached the front of the unit's queue (the Orders turn): asks the route and drives
void Start(const SimContext& context, int unit, std::uint32_t tag, float x, float y, float z);
bool Running(int unit, std::uint32_t tag);            // the unit's task is for this order
void End(const SimContext& context, int unit);        // takes off the speed cap, clears focus and rules params
// UnitDestroyed: ends the unit's task; the units attacking it are due at once
void UnitDestroyed(const SimContext& context, int unit);
void FrontChanged(int unit);                          // the unit's front order changed (command call-ins)
// after the unit turn: the attack-moves started this step to one point share the slowest ground member's speed
void CapGroups(const SimContext& context);

} // namespace AMove
} // namespace KT1

#endif
