/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: reading a unit's queue, and the busy-unit turn's classification (ord_orders.lua Order, EngineAhead,
// RS.Front, StripEngineWork, AssistStep, LeadWork, WorkOf and the walk in GameFrame). Pure functions over the queue the
// adapter read; the front entry is index 0. No engine include.
//
// The unit's order (ORD-004 v1 R6) is the first entry that is neither internal nor the engine's own work ahead of an
// internal fight step. Only the first QUEUE_READ entries are looked at.

#ifndef KT1_CORE_ORDERS_QUEUE_RULES_H
#define KT1_CORE_ORDERS_QUEUE_RULES_H

#include <cstddef>
#include <cstdint>
#include <span>

#include "OrdersState.h"

namespace KT1::Core::Orders {

constexpr int32_t QUEUE_READ = 8;    // DESIGN: entries read to find the order
constexpr int32_t GUARD_READ = 5;    // DESIGN: entries read of a guard and of its lead
constexpr int32_t WALK_READ = 12;    // DESIGN: entries read while a unit walks to a spot

bool IsEngineWork(CommandKind kind);   // attack, repair, reclaim, capture, resurrect, guard
// queue[i] is engine work with an internal fight step right behind it
bool EngineAhead(std::span<const OrderView> queue, int32_t i);
// internal, or engine work ahead of an internal fight
bool CountsInternal(std::span<const OrderView> queue, int32_t i);
int32_t OrderIndex(std::span<const OrderView> queue);   // the unit's order within QUEUE_READ entries; -1: none

// How the turn finds a busy unit's order from its first entry alone.
enum class TurnPath : uint8_t {
	OwnFightOrPatrol,   // its own fight or patrol in front: that is the order
	Plain,              // a non-builder's own plain order: that is the order, and the turn changes nothing for it
	ReadQueue,          // anything else: find the order in the queue
};
TurnPath PathOf(const OrderView& front, const DefFacts& def);
// the front entry is an attack-move order whose task must start (no task, or one for another tag)
bool StartsAMove(const OrderView& front, bool hasTask, uint32_t taskTag);

// May this entry ahead of a patrol or attack-move order be removed? Engine work, internal or ahead of a fight, not the
// family's own step; an attack stays unless it names one valid unarmed unit (ORD-004 v1 R2, R5).
bool RemovesEngineWork(std::span<const OrderView> queue, int32_t i, const OwnStep& own, bool oneValidUnitTarget,
                       bool targetArmed);

// The assist step of a guard: the repair, reclaim or guard just ahead of its guard order (after the entries of a walk),
// given by the family or pushed by the engine's guard. step -1 with guard set: the guard is in front. Both -1: something
// else is.
struct AssistFound {
	int32_t step = -1;
	int32_t guard = -1;
};
bool WasGiven(std::span<const GivenWork> given, CommandKind kind, float target);
AssistFound FindAssistStep(std::span<const OrderView> queue, const ReachWalk& walk, std::span<const GivenWork> given,
                           const OwnStep& own);
// the entry that is the lead's work: its guard under an assist step, else the first entry that is neither a walk step
// nor a follow step; -1: none
int32_t LeadWorkIndex(std::span<const OrderView> queue, const ReachWalk& walk, std::span<const GivenWork> given,
                      const OwnStep& own);

// What AllowCommand records of an order (ord_orders.lua RecordGiven): one-target repair and reclaim are remembered (an
// unqueued one forgets the rest); another unqueued order that is not an insert, remove or state change forgets all.
void RecordGiven(std::vector<GivenWork>& given, CommandKind kind, std::span<const float> params, bool shift);

// WorkOf: the work of the entry a lead is on. Reclaim parameters from maxUnits up name features.
struct WorkFacts {
	bool targetValid = false;       // parameter 1 names a valid unit
	float targetBuild = 1;          // its build progress
	UnitId building = NO_UNIT;      // what the lead is building now
	float buildingBuild = 1;
};
Task WorkOf(const OrderView& work, const WorkFacts& facts, int32_t maxUnits);

// ORD-005 v2 R1, ORD-004: the next check of a patrol or fight. The first check is at once; each unit then keeps its own
// phase within the interval, by unit id.
Step NextPatrolCheck(Step previous, Step now, Step interval, UnitId unit);
inline bool CheckDue(Step next, Step now) { return next == NO_STEP || now >= next; }

// the trace column front_class
FrontClass ClassifyFront(CommandKind orderKind, TurnPath path, bool assistFound, bool engineWorkInFront);

} // namespace KT1::Core::Orders

#endif
