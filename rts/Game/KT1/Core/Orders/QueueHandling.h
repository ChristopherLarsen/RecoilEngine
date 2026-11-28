/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: queue handling beside the turn (ord_orders.lua CommandFallback, UnitCmdDone, the transport load and
// set-down clocks, the ferry route and the self-destruct countdown; REQ ORD-001 v1 R2, MOV-020 v2, ORD-007 v2,
// ORD-010 v6 R4, ORD-011 v4). Pure decisions over the clocks in State. No engine include.

#ifndef KT1_CORE_ORDERS_QUEUE_HANDLING_H
#define KT1_CORE_ORDERS_QUEUE_HANDLING_H

#include <cstdint>
#include <span>
#include <vector>

#include "OrdersState.h"

namespace KT1::Core::Orders {

struct OrdersConfig;

// ---- the waypoint step -------------------------------------------------------------------------------------------

struct WaypointParams {
	float arrive = 0;                // the arrival radius, elmos
	float noProgress = 0;            // steps
	float progress = 0;              // elmos
	float nearRadii = 0;
	float anywhereFactor = 0;
	float restSpeed = 0;             // elmos per step
	float restSteps = 0;             // calls at rest, in steps
	float goalFloorShare = 0;
	float goalShare = 0;
	float same = 0;                  // elmos: a goal-cell record matches within this
	bool stepAll = false;
};
WaypointParams MakeWaypointParams(const OrdersConfig& config);

// the record whose stopping point is (restX, restZ)
bool FindGoalCell(std::span<const GoalCellRecord> records, float restX, float restZ, float same, GoalCellRecord& out);
void AddGoalCell(std::vector<GoalCellRecord>& records, const GoalCellRecord& record, int32_t keep);

struct WaypointFacts {
	float ux = 0, uz = 0;            // the unit
	float vx = 0, vz = 0;            // its velocity, elmos per step
	float x = 0, z = 0;              // the command's point (parameters 1 and 3)
	float orderedX = 0, orderedZ = 0;   // the ordered point (a goal-cell record's), else the command's
	float offset = 0;                // between the stopping point and the ordered point
	float radius = 0;                // the def's advance radius, else the arrival radius
	bool hasAdvance = false;         // the def has its own advance radius
	Step now = NO_STEP;
};
struct WaypointAnswer {
	bool finished = false;           // CommandFallback's second result (the first is always true)
	bool setGoal = false;            // set the unit's move goal at the command's point with goalRadius
	float goalRadius = 0;
};
WaypointAnswer RunWaypoint(WaypointStep& step, const WaypointFacts& facts, const WaypointParams& p);

// UnitCmdDone of a move: does a cut point need a step, and to where? (internal moves never do; a unit on the fork's move
// type only when more moves follow)
struct MoveDoneFacts {
	bool internal = false;
	bool ownMove = false;
	bool moreMoves = false;          // another move waits within the first MORE_MOVES_READ entries
	bool toOrdered = false;          // the def has an advance radius, or step_all: the step drives to the ordered point
	float ux = 0, uz = 0;
	float x = 0, y = 0, z = 0;       // the done command's point
	float radius = 0;
};
constexpr int32_t MORE_MOVES_READ = 4;   // DESIGN: entries looked at for a following move
bool PlanWaypointStep(const MoveDoneFacts& facts, std::span<const GoalCellRecord> records, const WaypointParams& p,
                      WaypointDue& due);

// ---- transports --------------------------------------------------------------------------------------------------

struct TransportParams {
	float stepsPerSecond = 0;
	float askGap = 0;                // steps
	float dropHold = 0;              // steps
	float reverseHold = 0;           // steps
	int32_t askHalf = 0;             // steps: half the engine's slow update
	bool reverseOrder = false;
};
TransportParams MakeTransportParams(const OrdersConfig& config);

struct TransportTimes {
	float loadS = 0;
	float unloadS = 0;
	float dropAccuracy = 0;          // elmos
	float intervalS = -1;            // DESIGN: -1 = no set-down interval
};
TransportTimes TimesFor(const OrdersConfig& config, const DefFacts& transport);

// a load or unload order reached the transport (UnitCommand): keeps the pickup order's clock over its hand-over, and
// ignores an order queued behind others
void NoteTransportOrder(OrderClock& clock, ClockKind kind, Step now, bool pickup, bool queuedBehind);
// the clock counts from the order for transports that unload all at once, else from the first ask
Step OrderStart(const OrderClock& clock, ClockKind kind, Step now, bool unloadAll);
// one ask of the engine; true: attach, or set down, now (the gate then ends)
bool AskLoad(GateClock& gate, UnitId cargo, Step now, Step orderStart, float loadS, const TransportParams& p);
bool AskUnload(GateClock& gate, UnitId cargo, Step now, Step orderStart, float unloadS, float intervalS,
               const TransportParams& p);
// keeps a hold alive; true while it lasts (refuse), false once it has run `holdSteps`
bool Holding(HoldClock& hold, Step now, float holdSteps, const TransportParams& p);
bool OffDropPoint(float x, float z, float orderX, float orderZ, float accuracy);

// ---- the ferry (ORD-007 v2 R4, R5) --------------------------------------------------------------------------------

struct FerryParams {
	Step period = 1;
	float leaveDelay = 0;            // steps
	float gather = 0;                // elmos
	float dropRadius = 0;
	float returnDistance = 0;
};
FerryParams MakeFerryParams(const OrdersConfig& config);

// `guarding`: the units guarding the marker within gather and not carried, ascending. Updates arrivals and `waiting`
// (arrival step, then id).
void NoteWaiting(FerryRoute& route, std::span<const UnitId> guarding, Step now);
enum class TripState : uint8_t {
	NotOnTrip,
	Busy,        // on the way out or unloading: nothing to do
	Unload,      // arrived with cargo and no order: order the set-down again
	Done,        // all set down: stop each of `trip`, then clear it and go on
};
TripState CheckTrip(const FerryRoute& route, bool queueEmpty, int32_t carried);
bool LeaveDue(const FerryRoute& route, int32_t carried, bool full, Step now, const FerryParams& p);
void BeginTrip(FerryRoute& route, std::span<const UnitId> carried, Step now);
bool ReturnNeeded(float x, float z, float markerX, float markerZ, const FerryParams& p);

// ---- self-destruct (ORD-010 v6 R4) --------------------------------------------------------------------------------

Step SelfDestructAt(Step now, float delayS);

} // namespace KT1::Core::Orders

#endif
