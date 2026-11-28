/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind QueueHandling.h (LUA-FREE H01; step S12 ports them).

#include "QueueHandling.h"

namespace KT1::Core::Orders {

WaypointParams MakeWaypointParams(const OrdersConfig&) { return {}; }
bool FindGoalCell(std::span<const GoalCellRecord>, float, float, float, GoalCellRecord&) { return false; }
void AddGoalCell(std::vector<GoalCellRecord>&, const GoalCellRecord&, int32_t) {}
WaypointAnswer RunWaypoint(WaypointStep&, const WaypointFacts&, const WaypointParams&) { return {}; }
bool PlanWaypointStep(const MoveDoneFacts&, std::span<const GoalCellRecord>, const WaypointParams&, WaypointDue&) {
	return false;
}

TransportParams MakeTransportParams(const OrdersConfig&) { return {}; }
TransportTimes TimesFor(const OrdersConfig&, const DefFacts&) { return {}; }
void NoteTransportOrder(OrderClock&, ClockKind, Step, bool, bool) {}
Step OrderStart(const OrderClock&, ClockKind, Step now, bool) { return now; }
bool AskLoad(GateClock&, UnitId, Step, Step, float, const TransportParams&) { return false; }
bool AskUnload(GateClock&, UnitId, Step, Step, float, float, const TransportParams&) { return false; }
bool Holding(HoldClock&, Step, float, const TransportParams&) { return false; }
bool OffDropPoint(float, float, float, float, float) { return false; }

FerryParams MakeFerryParams(const OrdersConfig&) { return {}; }
void NoteWaiting(FerryRoute&, std::span<const UnitId>, Step) {}
TripState CheckTrip(const FerryRoute&, bool, int32_t) { return TripState::NotOnTrip; }
bool LeaveDue(const FerryRoute&, int32_t, bool, Step, const FerryParams&) { return false; }
void BeginTrip(FerryRoute&, std::span<const UnitId>, Step) {}
bool ReturnNeeded(float, float, float, float, const FerryParams&) { return false; }

Step SelfDestructAt(Step, float) { return NO_STEP; }

} // namespace KT1::Core::Orders
