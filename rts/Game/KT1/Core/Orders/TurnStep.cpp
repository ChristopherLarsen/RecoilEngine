/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the busy-unit turn behind TurnStep.h, in ord_orders.lua GameFrame's order. No engine include.

#include "TurnStep.h"

#include <algorithm>

#include "GuardRules.h"
#include "OrdersConfig.h"
#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Orders {

namespace {

std::span<const OrderView> QueueOf(const TurnUnit& unit) {
	return {unit.queue.data(), std::min<size_t>(unit.numQueue, QUEUE_READ)};
}

bool EngineWorkAhead(const TurnUnit& unit, int32_t orderIndex) {
	for (int32_t i = 0; i < orderIndex; ++i) {
		const OrderView& c = unit.queue[i];
		const bool own = unit.own.kind != OwnKind::None && c.tag == unit.own.tag;
		if (IsEngineWork(c.kind) && !own)
			return true;
	}
	return false;
}

// EngWork: the reach check of a builder with an order, spread over units by id
ReachAction PlanReach(const TurnUnit& unit, bool assists, const TurnParams& params, Step now) {
	ReachParams reach;
	reach.every = params.reachEvery;
	reach.walkingEvery = params.reachWalkingEvery;
	if (!ReachCheckDue(unit.unit, now, unit.walking, reach) || !assists)
		return ReachAction::None;
	if (unit.walking)
		return ReachAction::CheckWalk;
	const OrderView& front = unit.queue[0];
	const bool work = front.kind == CommandKind::Repair || front.kind == CommandKind::Reclaim;
	return (work && front.numParams == 1) ? ReachAction::CheckReach : ReachAction::ForgetStall;
}

// the guard, fight and patrol branches: they alone keep a next check
void Schedule(TurnWork& work, const TurnUnit& unit, const TurnParams& params, Step now, Step& next) {
	const CommandKind order = work.front.order;
	const int32_t at = work.front.orderIndex;
	if (order == CommandKind::Guard && unit.queue[at].numParams >= 1) {
		work.guarded = unit.queue[at].params[0];
		work.orderTag = unit.queue[at].tag;
		if (CheckDue(next, now)) {
			next = now + params.guardCheck;
			work.guard = GuardAction::Check;
		} else if (unit.own.kind == OwnKind::Assist) {
			work.guard = GuardAction::Assist;
		}
		return;
	}
	if (order == CommandKind::Fight || order == CommandKind::Patrol) {
		work.stripBelow = at;
		if (CheckDue(next, now)) {
			const bool patrol = order == CommandKind::Patrol;
			next = NextPatrolCheck(next, now, patrol ? params.patrolCheck : params.fightCheck, unit.unit);
			work.patrolCheck = patrol;   // a fight's check only moves its clock
		}
		return;
	}
	next = NO_STEP;
}

void Apply(State& state, std::span<const TurnEvent> events) {
	for (const TurnEvent& e: events) {
		if (e.kind == TurnEventKind::Join)
			state.turn.Add(e.unit);
		else
			state.Forget(e.unit);
	}
}

// the port of the replay: the walk's reads come from the record; of its work only the joins and leaves it raised are done
class RecordPort {
public:
	RecordPort(State& state, const TurnInput& in, TurnOutput& out):
		state(state), in(in), out(out), used(in.units.size(), 0) {}

	const TurnUnit& Read(UnitId unit) {
		auto it = std::lower_bound(in.units.begin(), in.units.end(), unit,
		                           [](const TurnUnit& u, UnitId id) { return u.unit < id; });
		for (; it != in.units.end() && it->unit == unit; ++it) {
			uint8_t& seen = used[static_cast<size_t>(it - in.units.begin())];
			if (seen == 0) {
				seen = 1;
				return *it;
			}
		}
		out.missing.push_back(unit);
		empty = TurnUnit{};
		empty.unit = unit;
		return empty;
	}

	void Act(const TurnUnit& facts, const TurnWork&) {
		Apply(state, facts.during);
		if (facts.gone)
			state.Forget(facts.unit);
	}

	int32_t QueueLenAfter(const TurnUnit& facts) const { return facts.queueLenAfter; }

	void Done(const TurnWork& work) {
		const DenseIndex::Slot slot = state.SlotOf(work.unit);
		out.turnList.push_back(work.unit);
		out.frontClass.push_back(static_cast<uint8_t>(work.front.frontClass));
		out.nextCheck.push_back(slot == DenseIndex::NONE ? NO_STEP : state.nextCheck[slot]);
		out.work.push_back(work);
	}

private:
	State& state;
	const TurnInput& in;
	TurnOutput& out;
	std::vector<uint8_t> used;   // by record: a visit took it
	TurnUnit empty;
};

void ClearOutput(TurnOutput& out) {
	out.turnList.clear();
	out.frontClass.clear();
	out.nextCheck.clear();
	out.work.clear();
	out.listAfter.clear();
	out.missing.clear();
}

} // namespace

TurnParams MakeTurnParams(const OrdersConfig& config) {
	TurnParams p;
	p.guardCheck = Rate::Every(config.guard.check_s);
	p.patrolCheck = Rate::Every(config.patrol.patrol_check_s);
	p.fightCheck = Rate::Every(config.patrol.fight_check_s);
	p.safetyNet = Rate::Every(config.safety_net_s);
	p.reachEvery = Rate::Every(config.reach.every_s);
	p.reachWalkingEvery = Rate::Every(config.reach.walking_every_s);
	return p;
}

TurnInit MakeTurnInit(const OrdersConfig& config, std::span<const DefFacts> defs, int32_t maxUnits, float mapX,
                      float mapZ) {
	TurnInit init;
	init.stepsPerSecond = Rate::StepsPerSecond();
	init.maxUnits = maxUnits;
	init.mapX = mapX;
	init.mapZ = mapZ;
	init.guardCheckS = config.guard.check_s;
	init.patrolCheckS = config.patrol.patrol_check_s;
	init.fightCheckS = config.patrol.fight_check_s;
	init.safetyNetS = config.safety_net_s;
	init.reachEveryS = config.reach.every_s;
	init.reachWalkingEveryS = config.reach.walking_every_s;
	for (const DefFacts& def: defs)
		init.defFlags.push_back(def.flags);
	return init;
}

void ApplyTurnInit(const TurnInit& init, State& state, TurnParams& params) {
	Rate::SetStepsPerSecond(init.stepsPerSecond);
	OrdersConfig config;
	config.guard.check_s = init.guardCheckS;
	config.patrol.patrol_check_s = init.patrolCheckS;
	config.patrol.fight_check_s = init.fightCheckS;
	config.safety_net_s = init.safetyNetS;
	config.reach.every_s = init.reachEveryS;
	config.reach.walking_every_s = init.reachWalkingEveryS;
	const int32_t defs = static_cast<int32_t>(init.defFlags.size());
	state.Init(init.maxUnits, std::max(0, defs - 1), init.mapX, init.mapZ, config);
	for (int32_t d = 0; d < defs && d < static_cast<int32_t>(state.defs.size()); ++d)
		state.defs[d].flags = init.defFlags[d];
	params = MakeTurnParams(config);
}

void SetTurnQueue(TurnUnit& unit, std::span<const OrderView> queue) {
	unit.numQueue = static_cast<uint8_t>(std::min<size_t>(queue.size(), QUEUE_READ));
	unit.queue = {};
	std::copy_n(queue.begin(), unit.numQueue, unit.queue.begin());
}

TurnFront ClassifyUnit(const TurnUnit& unit, bool assists) {
	TurnFront front;
	const auto queue = QueueOf(unit);
	if (unit.queueLen == 0 || queue.empty())
		return front;
	DefFacts def;
	def.Set(DefFlag::Assists, assists);
	front.path = PathOf(queue[0], def);
	// DESIGN: entry 0 is the front
	const int32_t at = (front.path == TurnPath::ReadQueue) ? OrderIndex(queue) : 0;
	if (at < 0)
		return front;
	front.orderIndex = at;
	front.order = queue[at].kind;
	front.engineWork = EngineWorkAhead(unit, at);
	const bool mayAssist = front.order != CommandKind::Guard && front.order != CommandKind::Fight
	                       && front.order != CommandKind::Patrol;
	if (front.path == TurnPath::ReadQueue && mayAssist) {
		const AssistFound found = FindAssistStep(queue, unit.walk, unit.given, unit.own);
		if (found.step >= 0) {
			front.assistFound = true;
			front.orderIndex = found.guard;
			front.order = CommandKind::Guard;
		}
	}
	front.frontClass = ClassifyFront(front.order, front.path, front.assistFound, front.engineWork);
	return front;
}

void ApplySafetyNet(State& state, const TurnParams& params, Step now, std::span<const UnitId> busy) {
	if (now % params.safetyNet != 0)
		return;
	for (const UnitId unit: busy)
		state.turn.Add(unit);
}

TurnWork VisitUnit(State& state, const TurnParams& params, Step now, const TurnUnit& unit) {
	TurnWork work;
	work.unit = unit.unit;
	const DefFacts* def = state.Def(unit.defId);
	const bool assists = def != nullptr && def->Has(DefFlag::Assists);
	work.front = ClassifyUnit(unit, assists);
	const bool queued = unit.queueLen != 0 && unit.numQueue != 0;
	if (queued) {
		work.startAMove = StartsAMove(unit.queue[0], unit.hasTask, unit.taskTag);
		work.plain = work.front.path == TurnPath::Plain && unit.queue[0].kind != CommandKind::AMove;
	}
	if (work.front.order != NO_ORDER && !work.plain)
		work.reach = PlanReach(unit, assists, params, now);
	const DenseIndex::Slot slot = state.Track(unit.unit, unit.defId);
	if (slot == DenseIndex::NONE)
		return work;
	Schedule(work, unit, params, now, state.nextCheck[slot]);
	state.frontClass[slot] = static_cast<uint8_t>(work.front.frontClass);
	state.turnThisStep.push_back(unit.unit);
	return work;
}

void FinishUnit(State& state, int32_t& cursor, UnitId unit, int32_t queueLenAfter) {
	if (cursor >= state.turn.Size() || state.turn.At(cursor) != unit)
		return;   // gone during its turn: the slot holds another unit now
	if (queueLenAfter != 0) {
		++cursor;
		return;
	}
	const DenseIndex::Slot slot = state.SlotOf(unit);
	if (slot != DenseIndex::NONE)
		state.nextCheck[slot] = NO_STEP;
	state.turn.Remove(unit);
}

void RunTurnStep(State& state, TurnParams& params, const TurnInput& in, TurnOutput& out) {
	ClearOutput(out);
	if (in.hasInit)
		ApplyTurnInit(in.init, state, params);
	state.BeginStep();
	Apply(state, in.events);
	ApplySafetyNet(state, params, in.now, in.netUnits);
	RecordPort port(state, in, out);
	WalkTurns(state, params, in.now, port);
	for (int32_t i = 0; i < state.turn.Size(); ++i)
		out.listAfter.push_back(state.turn.At(i));
}

} // namespace KT1::Core::Orders
