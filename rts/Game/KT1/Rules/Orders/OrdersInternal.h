/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS: calls between the family's three systems (Orders here, AMove, OrderRules). Not for other families
// (OrdersApi.h). System Orders owns the state and the config and implements everything here.
//
// Phase order (Phase::FrameStart; the Lua's order inside one step):
//   2000  OrderRules   idle aircraft over water (mov_air_idle_water.lua, ahead of ord_orders.lua)
//   2100  Orders       dropped reclaims, self-destructs, move tracking, ferries, waypoint steps
//   2200  AMove        the attack-move tasks (the Lua runs them BEFORE the unit turn)
//   2300  Orders       the busy-unit turn (it starts new attack-moves), then AMove's group speed caps
//   2400  OrderRules   repeated queue trims of transferred factories
// Run order (KT1_REGISTER_SYSTEM): Orders keeps 200; AMove 2100; OrderRules 2200.

#ifndef KT1_RULES_ORDERS_INTERNAL_H
#define KT1_RULES_ORDERS_INTERNAL_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/Orders/OrderTypes.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"
#include "Game/KT1/Core/Orders/OrdersState.h"

struct Command;
class CUnit;

namespace KT1 {

struct SimContext;

namespace Orders {

constexpr int PHASE_IDLE_WATER = 2000;   // DESIGN: the orders above, in Phases::Add
constexpr int PHASE_PRE_TURN = 2100;     // DESIGN
constexpr int PHASE_AMOVE = 2200;        // DESIGN
constexpr int PHASE_TURN = 2300;         // DESIGN
constexpr int PHASE_TRANSFER = 2400;     // DESIGN
constexpr int RUN_ORDER_AMOVE = 2100;    // DESIGN: KT1Register.h, synced "before move"
constexpr int RUN_ORDER_RULES = 2200;    // DESIGN

namespace Internal {

// the family's config with the fixture in use over it (data/kt1/orders.json); the defaults before Orders has read it
const Core::Orders::OrdersConfig& Config();
Core::Orders::State* State();            // nullptr while system Orders is off

// the unit's queue as the engine holds it (a factory: its orders for new units), at most `max` entries; `out` is cleared
void ReadQueue(int unit, int max, std::vector<Core::Orders::OrderView>& out);
Core::Orders::CommandKind KindOf(int commandID);       // engine and game command ids to kinds
Core::Orders::OrderView ViewOf(const ::Command& command);

// An order the family gives: recorded in State::issued, given as synced code gives it (it passes the Allow chain).
// false: skipped (check mode, or no such unit).
bool Give(int unit, int commandID, std::span<const float> params, std::uint8_t options);
bool Remove(int unit, std::uint32_t tag);              // an engine remove of one entry
// ord_orders.lua Insert: an internal order put in front, remembered as the unit's own step when it took the front
void InsertAhead(int unit, int commandID, std::span<const float> params, Core::Orders::OwnKind kind, int target);
void SetMoveGoal(int unit, float x, float y, float z, float radius);

// ord_refuse.lua: rules params cmb_refused and cmb_refused_frame (the HUD shows the refusal once)
void Refuse(int unit, const char* reason, int frame);

// what OrderRules' native rows do to Orders' state (a validator has no SimContext, so these take none)
// rules param fire_state, the engine's fire-state order (State::settingFire while it is given) and the unit's button
void SetFireState(int unit, Core::Orders::FireState state);
void NoteGiven(int unit, const ::Command& command);    // RecordGiven
void ToggleSelfDestruct(int unit, int frame);          // starts the countdown, or cancels a running one
void CancelSelfDestruct(int unit);                     // a stop
void SetFerryRoute(int transport, std::span<const float> params, int team, int frame);   // marker, route, stop, OrderAck
void EndFerry(int transport);                          // removes the marker without a blast
void SetDive(int unit, bool submerged);                // the move type and the button
void NoteLivingReclaim(int builder, int target);       // a reclaim step on a unit that may not take it
// the transport gates (the answer of AllowUnitTransportLoad and Unload)
bool AskTransportLoad(const CUnit& transport, const CUnit& cargo, bool engineAllows, int frame);
bool AskTransportUnload(const CUnit& transport, const CUnit& cargo, float x, float z, bool engineAllows, int frame);

} // namespace Internal
} // namespace Orders
} // namespace KT1

#endif
