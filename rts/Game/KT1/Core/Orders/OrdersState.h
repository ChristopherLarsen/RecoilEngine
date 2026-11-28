/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the family's per-unit state beside one DenseIndex, and the few lists beside it. System Orders owns it;
// AMove and OrderRules reach it through Rules/Orders/OrdersInternal.h. The logic is free functions: QueueRules.h (the
// queue and the busy-unit turn), MoveTracking.h, GuardRules.h (patrol, guard, assist, reach) and QueueHandling.h
// (waypoint step, transports, ferry, self-destruct). No engine include.
//
// Order: the unit turn walks TurnList in its own order (the Lua's busy list: join at the end, a leaver's slot takes the
// last). Every other walk is one of the ascending id lists below.
// Allocation: Init sizes the columns; a unit's variable lists allocate when first used.

#ifndef KT1_CORE_ORDERS_STATE_H
#define KT1_CORE_ORDERS_STATE_H

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "DefFacts.h"
#include "Grids.h"
#include "OrderTypes.h"
#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"

namespace KT1::Core::Orders {

struct OrdersConfig;

constexpr size_t REACH_TRIED = 8;    // DESIGN: most spots remembered per stalled builder (ReachConfig::tries)
constexpr size_t WALK_ORDERS = 8;    // DESIGN: most move steps of one walk to a spot
constexpr size_t FERRY_PARAMS = 6;   // DESIGN: start and end point of a route

// ours[u]: the step put ahead of the unit's order
struct OwnStep {
	uint32_t tag = 0;
	OwnKind kind = OwnKind::None;
	int32_t target = NO_UNIT;        // a unit id, or the reclaim parameter of a wreck
};

enum class Reach : uint8_t { Undecided, Reachable, Unreachable };   // the fill's verdict (the Lua's nil, true, false)

// moves[u]: one tracked move
struct MoveTrack {
	bool active = false;
	uint32_t tag = 0;
	float gx = 0, gz = 0;            // the goal
	Step start = NO_STEP;
	bool hasStartPos = false;        // false for a move the family gave itself
	float sx = 0, sz = 0;            // where the unit stood when tracking began
	bool hasBest = false;
	float best = 0;                  // least distance to the goal so far
	Step bestAt = NO_STEP;
	Step still = NO_STEP;            // standing since; NO_STEP: moving
	Step nextReach = NO_STEP;
	Step reachAt = NO_STEP;          // the last fill; NO_STEP: none yet
	Reach lastReach = Reach::Undecided;
	float rx = 0, rz = 0;            // where the unit stood at the last fill
	bool unreach = false;            // the fill said unreachable
	bool skipped = false;            // no fill was run: it is owed once the unit stands
	bool failOnEnd = false;          // a redirected move: it ends as completed-with-failure
	bool hasAim = false;
	float aimX = 0, aimZ = 0;        // the redirect's move point
	int32_t tries = 0;               // times a dropped redirect was given again
	bool hasAnchor = false;
	float ax = 0, az = 0;            // the last position of progress
	Step axAt = NO_STEP;
};

struct StopPoint {                   // rules params ord_stop_x, ord_stop_z
	bool set = false;
	float x = 0, z = 0;
};

struct WaypointDue {                 // a cut move point to finish: the step is inserted at the next turn
	bool active = false;
	float x = 0, y = 0, z = 0;
};

struct WaypointStep {                // the waypoint step being driven (CommandFallback)
	bool active = false;
	float x = 0, z = 0;              // the step's own point (its parameters 1 and 3)
	Step since = NO_STEP;
	bool hasBest = false;
	float best = 0;
	Step bestAt = NO_STEP;
	int32_t still = 0;               // calls in a row at rest
};

// GG.GoalCellOrdered: OrderRules' goal-cell row writes it, the waypoint step reads it (MOV-020 R2)
struct GoalCellRecord {
	float restX = 0, restZ = 0;      // the stopping point the unit was sent to
	float x = 0, y = 0, z = 0;       // the ordered point
};

struct GivenWork {                   // a repair or reclaim order given to the unit (not pushed by the engine)
	CommandKind kind = CommandKind::Other;
	float target = 0;
};

struct ReachStall {                  // a builder's repair or reclaim target out of reach
	bool active = false;
	CommandKind kind = CommandKind::Other;
	float target = 0;                // the order's one parameter
	float best = 0;
	Step since = NO_STEP;
	int32_t tries = 0;
	int32_t tried = 0;
	std::array<std::array<float, 2>, REACH_TRIED> spots{};
	bool walked = false;             // walkFrom and walkFor are set
	Step walkFrom = NO_STEP;
	float walkFor = 0;               // steps (a float, as the Lua keeps it)
};

struct ReachWalk {                   // the move steps of a walk to a spot, by tag
	int32_t count = 0;
	std::array<uint32_t, WALK_ORDERS> tags{};
	bool Has(uint32_t tag) const;
};

enum class ClockKind : uint8_t { None, Load, Unload };

struct OrderClock {                  // orderAt[t]: the transport's current load or unload order
	ClockKind kind = ClockKind::None;
	Step given = NO_STEP;
	bool pickup = false;             // it is the pickup order (CUSTOM_PICKUP)
};

struct GateClock {                   // loading[t], unloading[t]
	bool active = false;
	UnitId cargo = NO_UNIT;
	Step start = NO_STEP;
	Step last = NO_STEP;
	bool fromOrder = false;
	bool hasInterval = false;        // an unload paced by the transport's own interval
	float intervalS = 0;
	int32_t setDown = 0;             // units set down under the interval
};

struct HoldClock {                   // dropHold[t], revHold[t]
	bool active = false;
	Step since = NO_STEP;
	Step last = NO_STEP;
};

struct FerryArrival {
	UnitId unit = NO_UNIT;
	Step step = NO_STEP;
};

struct FerryRoute {                  // ferries[t]
	bool active = false;
	UnitId marker = NO_UNIT;
	float ex = 0, ey = 0, ez = 0;    // the route's end
	std::vector<FerryArrival> arrived;   // ascending unit id
	std::vector<UnitId> waiting;     // in arrival order
	std::vector<UnitId> trip;        // the units taken on the way out
	bool onTrip = false;
	Step lastArrival = NO_STEP;
	Step leftAt = NO_STEP;
};

struct Failure {                     // GG.Ord.failures (test instruments)
	UnitId unit = NO_UNIT;
	Step step = NO_STEP;
	FailReason why = FailReason::Unreachable;
};

struct FallbackRecord {              // trace column fallback_result
	UnitId unit = NO_UNIT;
	int32_t command = 0;
	bool used = false;
	bool finished = false;
};

// The busy units, in the order the turn visits them. Add appends; Remove puts the last unit in the leaver's place, so a
// walk that removes the unit at `i` visits slot `i` again.
class TurnList {
public:
	void Init(int32_t maxUnits);
	bool Add(UnitId unit);           // false: already in it
	bool Remove(UnitId unit);        // false: not in it
	bool Has(UnitId unit) const;
	int32_t Size() const { return static_cast<int32_t>(list.size()); }
	UnitId At(int32_t i) const { return list[i]; }

private:
	std::vector<UnitId> list;
	std::vector<int32_t> at;         // by unit id; -1: not in the list
};

class State {
public:
	// every column is sized here; defCount is the number of unit defs (ids 1 .. defCount); map sizes in elmos
	void Init(int32_t maxUnits, int32_t defCount, float mapX, float mapZ, const OrdersConfig& config);
	DenseIndex::Slot Track(UnitId unit, int32_t defID);   // the unit's slot, added when new
	DenseIndex::Slot SlotOf(UnitId unit) const { return units.index.SlotOf(unit); }
	// UnitDestroyed: drops every entry of the unit. The adapter first ends its ferry and its attack-move.
	void Forget(UnitId unit);
	void BeginStep();                // empties the per-step records
	const DefFacts* Def(int32_t defID) const;             // nullptr: no such def
	const DefFacts* DefOf(UnitId unit) const;             // nullptr: unit not tracked

	SimHost::UnitColumns units;
	Column<int32_t> defId;

	// the busy-unit turn
	Column<uint8_t> frontClass;      // FrontClass
	Column<Step> nextCheck;          // patrol and guard checks; NO_STEP: not set
	Column<OwnStep> own;
	Column<Step> stripped;           // engine work was last removed (Refused)
	Column<UnitId> guardRepair;      // GG.Ord.guardRepair: the guarded unit this builder repairs
	Column<UnitId> guardShield;      // GG.Ord.guardShield: the guarded unit whose shield it tops up
	Column<std::vector<GivenWork>> given;
	Column<ReachStall> stall;
	Column<ReachWalk> walk;

	// tracking
	Column<MoveTrack> move;
	Column<Step> fresh;              // a ground mover's move was given at this step
	Column<StopPoint> stop;
	Column<Step> failedAt;           // rules param order_failed

	// queue handling and states
	Column<WaypointDue> waypointDue;
	Column<WaypointStep> waypointStep;
	Column<std::vector<GoalCellRecord>> goalCells;
	Column<uint8_t> fireState;       // FireState; rules param fire_state
	Column<Step> selfDestruct;       // the step the unit dies (rules param selfd_frame); NO_STEP: none
	Column<UnitId> livingReclaim;    // by builder: a reclaim step on a unit that may not take it, seen last step
	Column<OrderClock> orderClock;
	Column<GateClock> loading;
	Column<GateClock> unloading;
	Column<HoldClock> dropHold;
	Column<HoldClock> reverseHold;
	Column<std::vector<UnitId>> loadOrder;   // cargo in the order attached (only reverse_order reads it)
	Column<FerryRoute> ferry;        // by transport

	TurnList turn;
	// the units with an entry above, ascending
	SortedIdList moveUnits;
	SortedIdList freshUnits;
	SortedIdList shieldUnits;
	SortedIdList waypointUnits;
	SortedIdList selfDestructUnits;
	SortedIdList reclaimUnits;
	SortedIdList ferryUnits;

	ChangeGrid changes;
	StructureGrid structures;
	std::vector<DefFacts> defs;      // by unit def id; the adapter fills it at Start

	// set while the family gives an order of its own that a row of the Allow chain must let through
	bool settingFire = false;        // the engine fire-state order of SetFireState (the EngineFireState row)
	bool givingFerryOrder = false;   // an order to a ferry transport (the EndsFerry row)

	int32_t fillSquaresUsed = 0;     // this tracking pass's order fills
	int8_t routeEngine = -1;         // the fork's route follower ends blocked moves: -1 not known yet, 0 no, 1 yes

	// per step: the trace columns turn_list, orders_given and fallback_result
	std::vector<UnitId> turnThisStep;
	std::vector<OrderIssue> issued;
	std::vector<FallbackRecord> fallbacks;

	std::vector<Failure> failures;   // dev instrument, never compared
};

} // namespace KT1::Core::Orders

#endif
