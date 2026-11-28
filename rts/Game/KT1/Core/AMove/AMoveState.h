/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 AMove core: the attack-move task (REQ ORD-003 v1 R3 to R5; ord_orders.lua SetMode, EndAMove, AMoveFrame,
// AMoveTick, AMoveStart, CapGroups, AMoveArrived): one task per unit holding an attack-move order, its modes, its due
// steps on a DueWheel and the events a tick raises. No engine include.
//
// Order: the units of one step are visited ascending (TakeDue). A unit is looked at when its upkeep or its pick is due,
// while it has a builder step (or any step of the family) ahead of the order, and when its front order changed.

#ifndef KT1_CORE_AMOVE_STATE_H
#define KT1_CORE_AMOVE_STATE_H

#include <cstdint>
#include <span>
#include <vector>

#include "AMoveRoute.h"
#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Common/DueWheel.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/Orders/DefFacts.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"

namespace KT1::Core::Orders {
struct OrdersConfig;
}

namespace KT1::Core::AMove {

using Orders::Step;
using Orders::UnitId;

enum class Mode : uint8_t { Route, Fight, Halt, Back };
int32_t ModeParam(Mode mode);        // rules param amove_state: 1 fighting, 2 halted, else 0

// what a tick raised (trace column events; Spring.KT1.AMove.Events() until step S04 takes the pick native)
enum class EventKind : uint8_t { PickDue, Arrived, FrontLost, BuilderStep };
const char* EventName(EventKind kind);   // "pick_due", "arrived", "front_lost", "builder_step"
struct Event {
	UnitId unit = Orders::NO_UNIT;
	EventKind kind = EventKind::PickDue;
};

struct Goal {
	float x = 0, y = 0, z = 0;
};
struct Resume {                      // where the unit left the route to fight or halt
	bool set = false;
	float x = 0, z = 0;
};
struct Headway {                     // the best squared distance so far and when it was reached
	bool set = false;
	float best = 0;
	Step at = Orders::NO_STEP;
};
// bits of State::flags
constexpr uint8_t HELD = 1;          // stopped at the weapon's reach
constexpr uint8_t PRIMARY = 2;       // the pick was made by weapon 1
constexpr uint8_t STEPPING = 4;      // a builder step is ahead of the order
constexpr uint8_t UPKEEP_PHASED = 8;
constexpr uint8_t PICK_PHASED = 16;

struct Attacker {                    // GG.Ord.amTargetOf
	UnitId target = Orders::NO_UNIT;
	UnitId unit = Orders::NO_UNIT;
};
struct Started {                     // an attack-move of a ground unit started this step, for the group's speed cap
	uint64_t group = 0;              // GroupKey of the goal
	UnitId unit = Orders::NO_UNIT;
};

struct Params {
	Step tick = 1;                   // upkeep, and the pick of a unit with no scanned weapon
	Step seenReuse = 1;
	float backStall = 0;             // steps
	float headway = 0;               // elmos squared
	float arrive = 0;                // elmos
	float airArrive = 0;             // elmos
	float groupNear = 0;             // radii
	float stopShare = 0;
	float holdGoal = 0;              // elmos
	float backGoal = 0;              // elmos
	float guardFactor = 0;           // strip half width = this x primary weapon range
	float seenCell = 0;              // elmos
	float seenSlack = 0;             // how far the fastest def moves in a reuse period
	float circleSpacing = 0;         // share of the half width
	float circleRadius = 0;
	int32_t directPerCircle = 0;
};
Params MakeParams(const Orders::OrdersConfig& config, std::span<const Orders::DefFacts> defs);
// the strip half width of a def; 0: it has none (unarmed, or no primary weapon)
float GuardRadius(const Orders::DefFacts& def, const Params& p);
StripShape ShapeFor(float guardRadius, const Params& p);

class State {
public:
	void Init(int32_t maxUnits, int32_t allyTeams, const Params& p);
	// a new task in mode Route, due at once; an old one of the unit is replaced
	DenseIndex::Slot Start(UnitId unit, uint32_t tag, const Goal& goal, Step now);
	void End(UnitId unit);                                   // drops the task and its attacker entry
	DenseIndex::Slot SlotOf(UnitId unit) const { return units.index.SlotOf(unit); }
	bool Running(UnitId unit, uint32_t tag) const;
	void SetMode(UnitId unit, Mode mode, UnitId target);     // keeps `attackers`
	std::span<const Attacker> AttackersOf(UnitId target) const;
	void TargetDied(UnitId target);                          // the units on it are due at once
	void MarkFrontChanged(UnitId unit);
	void Schedule(UnitId unit);                              // after due or pickDue changed
	// the units to look at this step, ascending and unique (see the order note above)
	void TakeDue(Step now, std::span<const UnitId> withOwnStep, std::vector<UnitId>& out);
	void NoteStarted(UnitId unit, float gx, float gz);
	void BeginStep();                                        // empties events and started

	SimHost::UnitColumns units;
	Column<uint8_t> mode;            // Mode; rules param amove_state
	Column<Goal> goal;
	Column<uint32_t> tag;            // the order's tag
	Column<UnitId> target;           // rules param amove_target
	Column<Resume> resume;
	Column<Step> due;                // the next upkeep
	Column<Step> pickDue;            // the next pick
	Column<uint8_t> flags;
	Column<float> uncappedSpeed;     // the unit's own speed while the group's cap is on; under 0: not capped
	Column<Headway> back;            // headway on the way back to the route
	Column<Headway> arrival;         // headway near the goal
	Column<Route> route;

	DueWheel wheel;
	SortedIdList changed;            // front order changed since the last step
	SortedIdList stepping;           // units with STEPPING set
	SeenCache seen;
	std::vector<Attacker> attackers; // ascending (target, unit)
	std::vector<Started> started;    // in start order
	std::vector<Event> events;       // this step's, ascending unit id
};

// ---- the task's decisions ------------------------------------------------------------------------------------------

// the next upkeep and pick: the first after at once, then a phase of its own within the tick, by unit id
Step NextUpkeep(uint8_t& flags, UnitId unit, Step now, Step tick);
Step NextPick(uint8_t& flags, UnitId unit, Step now, Step tick);

// mode Fight: drop (dead, cannot engage, or out of the strip), else test the line of fire within reach, else close in
enum class FightPlan : uint8_t { Drop, TryLine, Close };
FightPlan PlanFight(bool canEngage, bool nearRoute, float distSq, float reach);
float CloseRadius(float reach, const Params& p);   // the move goal radius while closing, at least 1 elmo

// at the goal (mode Route only): within the arrival radius, or near it without headway
bool Arrived(Headway& arrival, Mode mode, float distSq, bool air, Step now, const Params& p);
// mode Back, farther than the arrival radius from the resume point: still making headway (keep returning)
bool StillReturning(Headway& back, float distSq, Step now, const Params& p);

// The pick's order: the primary weapon's targets first, then the weapon's rank, then distance; on a full tie the one
// offered first (ascending id) stays.
struct PickKey {
	int32_t group = 0;               // 0: weapon 1 takes it, 1: only another weapon
	int32_t rank = 0;
	float distSq = 0;
};
bool Beats(const PickKey& candidate, const PickKey& best);

// attack-moves started together to one point share a speed cap: the goal to 0.1 elmo
uint64_t GroupKey(float gx, float gz);

} // namespace KT1::Core::AMove

#endif
