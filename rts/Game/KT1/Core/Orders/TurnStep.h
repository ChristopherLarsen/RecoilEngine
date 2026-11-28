/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the busy-unit turn as one step (ord_orders.lua GameFrame, the walk over ORDB; REQ ORD-004 v1,
// ORD-005 v2 R1). The live adapter and the offline replay (H02) run the same loop, WalkTurns: only the port differs.
//
// Declared state: in State turn (ORDB), nextCheck, frontClass, turnThisStep and defs; beside it TurnParams. The recorded
// step (RunTurnStep) reads that state and its TurnInput, nothing else: the first record carries TurnInit, which sets
// both up. The step writes no other column and gives no order: TurnWork says what the caller must do for each unit.
//
// A trace starts at the first step of the game, with one config: a fixture switched during a run needs its own trace. A
// trace that starts later also sends, with its TurnInit, a Join for every unit of the list in the list's order, and its
// reader sets State::nextCheck beside them (not carried here).
//
// Trace columns and the serializers of TurnInput: TurnTrace.h. No engine include.

#ifndef KT1_CORE_ORDERS_TURN_STEP_H
#define KT1_CORE_ORDERS_TURN_STEP_H

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "OrdersState.h"
#include "QueueRules.h"

namespace KT1::Core::Orders {

struct OrdersConfig;

constexpr CommandKind NO_ORDER = CommandKind::Count;   // the unit has no order (the Lua's id == nil)

// the periods of the walk, in steps
struct TurnParams {
	Step guardCheck = 1;             // guard_check_interval
	Step patrolCheck = 1;            // patrol_check_interval
	Step fightCheck = 1;             // fight_check_interval
	Step safetyNet = 1;              // H.Every(0.27)
	Step reachEvery = 1;             // REACH_EVERY
	Step reachWalkingEvery = 1;      // H.Every(0.1)
};
TurnParams MakeTurnParams(const OrdersConfig& config);   // needs Core::Rate

// what is fixed for a run, in seconds as the config holds it
struct TurnInit {
	int32_t stepsPerSecond = 0;      // Game.gameSpeed
	int32_t maxUnits = 0;            // Game.maxUnits
	float mapX = 0, mapZ = 0;        // Game.mapSizeX, Game.mapSizeZ
	float guardCheckS = 0;           // OV.ord.guard_check_interval
	float patrolCheckS = 0;          // OV.ord.patrol_check_interval
	float fightCheckS = 0;           // OV.ord.fight_check_interval
	float safetyNetS = 0;            // the 0.27 of H.Every
	float reachEveryS = 0;           // the 0.5 of REACH_EVERY
	float reachWalkingEveryS = 0;    // the 0.1 of H.Every
	std::vector<uint32_t> defFlags;  // DefFacts::flags by unit def id, 0 .. defCount; the walk reads Assists
};
// from the family's config (the fixture in use over it) and State::defs; needs Core::Rate
TurnInit MakeTurnInit(const OrdersConfig& config, std::span<const DefFacts> defs, int32_t maxUnits, float mapX,
                      float mapZ);
// sets the sim rate, sizes `state` afresh, fills its defs' flags and derives `params`
void ApplyTurnInit(const TurnInit& init, State& state, TurnParams& params);

enum class TurnEventKind : uint8_t {
	Join,      // ORDB.Add from UnitCommand
	Leave,     // UnitDestroyed: out of ORDB, and nothing is kept of it
};
struct TurnEvent {
	TurnEventKind kind = TurnEventKind::Join;
	UnitId unit = NO_UNIT;
};

// What the walk reads of one unit at its turn.
struct TurnUnit {
	UnitId unit = NO_UNIT;           // u
	int32_t defId = 0;               // Spring.GetUnitDefID(u); isBuilderDef is State::defs[defId]'s Assists
	int32_t queueLen = 0;            // the row's B[k + 3] (Spring.GetUnitCommandCount(u)) before its turn
	int32_t queueLenAfter = 0;       // CommandCount(u) after its turn; not read for a plain unit
	bool gone = false;               // destroyed during its turn, with no Leave in `during` (the live adapter sends one)
	uint8_t numQueue = 0;            // entries in queue
	std::array<OrderView, QUEUE_READ> queue{};   // Spring.GetUnitCommands(u, 8), front first
	OwnStep own;                     // ours[u]; kind None: nil
	bool walking = false;            // reachTag[u] ~= nil
	ReachWalk walk;                  // the tags of reachTag[u]
	std::vector<GivenWork> given;    // given[u], one per "cmd:target" key
	bool hasTask = false;            // amove[u] ~= nil
	uint32_t taskTag = 0;            // amove[u].tag
	std::vector<TurnEvent> during;   // the joins and leaves while its turn ran, in the order the call-ins came
};
// fills queue and numQueue, at most QUEUE_READ entries
void SetTurnQueue(TurnUnit& unit, std::span<const OrderView> queue);

// the unit's order as the turn finds it, before it does anything
struct TurnFront {
	TurnPath path = TurnPath::ReadQueue;
	FrontClass frontClass = FrontClass::None;
	CommandKind order = NO_ORDER;    // id
	int32_t orderIndex = -1;         // DESIGN: -1, none; the queue entry of the order (an assisting unit: of its guard)
	bool assistFound = false;        // AssistStep(u, q) found a step ahead of the guard, which becomes the order
	bool engineWork = false;         // engine work, not the family's own step, lies ahead of the order
};
TurnFront ClassifyUnit(const TurnUnit& unit, bool assists);   // assists: isBuilderDef of its def

enum class ReachAction : uint8_t {           // EngWork
	None,
	CheckWalk,
	CheckReach,      // the front is a repair or reclaim of one target
	ForgetStall,     // stall[u] = nil
};
enum class GuardAction : uint8_t {
	None,
	Check,           // CheckGuard: the check is due
	Assist,          // CheckAssist: assisting units are looked at every step
};

// What the caller does for one unit, in this order: start the attack-move, the reach work, then the guard work, or the
// strip and then the patrol check.
struct TurnWork {
	UnitId unit = NO_UNIT;
	TurnFront front;
	bool plain = false;              // the turn changes nothing: the queue length before it still holds
	bool startAMove = false;         // AMoveStart with queue[0]'s tag and point
	ReachAction reach = ReachAction::None;
	GuardAction guard = GuardAction::None;
	float guarded = 0;               // order.params[1] of the guard order
	uint32_t orderTag = 0;           // the guard order's tag
	int32_t stripBelow = 0;          // fight or patrol: test entries 0 .. stripBelow-1 with RemovesEngineWork
	bool patrolCheck = false;        // CheckPatrol is due

	// anything for the caller to do
	bool Acts() const {
		return startAMove || reach != ReachAction::None || guard != GuardAction::None || stripBelow > 0 || patrolCheck;
	}
};

// ORDB's safety net: on its steps, every unit of `busy` (those with a queue, in the order the walk reads them) joins
void ApplySafetyNet(State& state, const TurnParams& params, Step now, std::span<const UnitId> busy);
// one unit's turn: tracks the unit, writes its nextCheck and frontClass, records it in turnThisStep
TurnWork VisitUnit(State& state, const TurnParams& params, Step now, const TurnUnit& unit);
// after the caller's work: an empty queue takes the unit out (the slot at `cursor` then holds the last unit) and clears
// its nextCheck; else the cursor moves on. A unit that left during its turn leaves the cursor where it is.
void FinishUnit(State& state, int32_t& cursor, UnitId unit, int32_t queueLenAfter);

// The walk. Port:
//   const TurnUnit& Read(UnitId);                 valid until the next Read
//   void Act(const TurnUnit&, const TurnWork&);   the turn's work; it may join units or drop the unit
//   int32_t QueueLenAfter(const TurnUnit&);       asked for a unit that is not plain
//   void Done(const TurnWork&);                   after FinishUnit
template<class Port> void WalkTurns(State& state, const TurnParams& params, Step now, Port& port) {
	for (int32_t cursor = 0; cursor < state.turn.Size(); ) {
		const UnitId unit = state.turn.At(cursor);
		const TurnUnit& facts = port.Read(unit);
		const TurnWork work = VisitUnit(state, params, now, facts);
		port.Act(facts, work);
		const int32_t after = work.plain ? facts.queueLen : port.QueueLenAfter(facts);
		FinishUnit(state, cursor, unit, after);
		port.Done(work);
	}
}

// ---- the whole step from recorded values ------------------------------------------------------------------------

struct TurnInput {
	bool hasInit = false;            // the first record: ApplyTurnInit before the events
	TurnInit init;
	Step now = 0;                    // f
	std::vector<TurnEvent> events;   // since the previous walk, in the order the call-ins came (later ones: TurnUnit::during)
	std::vector<UnitId> netUnits;    // units with a queue, in the order the walk reads them (Busy: ascending); safety-net steps
	// every unit the walk visits, ascending unit id; a unit visited twice (it left and joined again during the walk) has a
	// record per visit, in the order of the visits
	std::vector<TurnUnit> units;
};

struct TurnOutput {
	std::vector<UnitId> turnList;    // the units given a turn, in order
	std::vector<uint8_t> frontClass; // FrontClass, one per visit
	std::vector<Step> nextCheck;     // nextCheck[u] after its turn; NO_STEP: nil
	std::vector<TurnWork> work;      // one per visit
	std::vector<UnitId> listAfter;   // ORDB.list after the walk
	std::vector<UnitId> missing;     // visited with no record in the input (read as an empty queue)
};

// init (when sent), events, safety net, walk. Reads nothing but `state`, `params` and `in`; `out` is overwritten.
void RunTurnStep(State& state, TurnParams& params, const TurnInput& in, TurnOutput& out);

} // namespace KT1::Core::Orders

#endif
