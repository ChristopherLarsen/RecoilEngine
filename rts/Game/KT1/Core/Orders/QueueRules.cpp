/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the queue rules behind QueueRules.h, as ord_orders.lua has them. No engine include.

#include "QueueRules.h"

#include <algorithm>

namespace KT1::Core::Orders {

namespace {

bool IsOwn(const OwnStep& own, const OrderView& entry) { return own.kind != OwnKind::None && entry.tag == own.tag; }

bool IsRepairOrReclaim(CommandKind kind) { return kind == CommandKind::Repair || kind == CommandKind::Reclaim; }

} // namespace

bool IsEngineWork(CommandKind kind) {
	switch (kind) {
		case CommandKind::Attack:
		case CommandKind::Repair:
		case CommandKind::Reclaim:
		case CommandKind::Capture:
		case CommandKind::Resurrect:
		case CommandKind::Guard: return true;
		default: return false;
	}
}

bool EngineAhead(std::span<const OrderView> queue, int32_t i) {
	const int32_t size = static_cast<int32_t>(queue.size());
	if (i < 0 || i + 1 >= size)
		return false;
	const OrderView& next = queue[i + 1];
	return IsEngineWork(queue[i].kind) && next.kind == CommandKind::Fight && next.internal;
}

bool CountsInternal(std::span<const OrderView> queue, int32_t i) { return queue[i].internal || EngineAhead(queue, i); }

int32_t OrderIndex(std::span<const OrderView> queue) {
	// the Lua reads this many entries, so the last one has nothing behind it
	const auto read = queue.first(std::min<size_t>(queue.size(), QUEUE_READ));
	for (int32_t i = 0; i < static_cast<int32_t>(read.size()); ++i) {
		if (!CountsInternal(read, i))
			return i;
	}
	return -1;   // DESIGN: -1, no order
}

TurnPath PathOf(const OrderView& front, const DefFacts& def) {
	const bool fightOrPatrol = front.kind == CommandKind::Fight || front.kind == CommandKind::Patrol;
	if (fightOrPatrol && !front.internal)
		return TurnPath::OwnFightOrPatrol;
	if (front.kind != CommandKind::Guard && !IsEngineWork(front.kind) && !front.internal && !def.Has(DefFlag::Assists))
		return TurnPath::Plain;
	return TurnPath::ReadQueue;
}

bool StartsAMove(const OrderView& front, bool hasTask, uint32_t taskTag) {
	return front.kind == CommandKind::AMove && !(hasTask && taskTag == front.tag);
}

bool RemovesEngineWork(std::span<const OrderView> queue, int32_t i, const OwnStep& own, bool oneValidUnitTarget,
                       bool targetArmed) {
	const OrderView& entry = queue[i];
	if (!IsEngineWork(entry.kind) || IsOwn(own, entry) || !CountsInternal(queue, i))
		return false;
	if (entry.kind != CommandKind::Attack)
		return true;   // builders' side work
	return oneValidUnitTarget && !targetArmed;
}

bool WasGiven(std::span<const GivenWork> given, CommandKind kind, float target) {
	return std::any_of(given.begin(), given.end(),
	                   [&](const GivenWork& g) { return g.kind == kind && g.target == target; });
}

AssistFound FindAssistStep(std::span<const OrderView> queue, const ReachWalk& walk, std::span<const GivenWork> given,
                           const OwnStep& own) {
	const int32_t size = static_cast<int32_t>(queue.size());
	for (int32_t i = 0; i < size; ++i) {
		const OrderView& c = queue[i];
		if (c.kind == CommandKind::Guard && !c.internal)
			return {-1, i};   // DESIGN: -1, the guard itself is in front
		if (walk.Has(c.tag))
			continue;
		const bool stepKind = IsRepairOrReclaim(c.kind) || c.kind == CommandKind::Guard;
		const bool guardBehind = i + 1 < size && queue[i + 1].kind == CommandKind::Guard;
		if (!stepKind || c.numParams != 1 || !guardBehind || !(c.internal || !WasGiven(given, c.kind, c.params[0])))
			return {};
		// not the family's own repair of the guarded unit (ORD-005 R4) or attack
		if (IsOwn(own, c) && own.kind != OwnKind::Assist)
			return {};
		return {i, i + 1};
	}
	return {};
}

int32_t LeadWorkIndex(std::span<const OrderView> queue, const ReachWalk& walk, std::span<const GivenWork> given,
                      const OwnStep& own) {
	if (queue.empty())
		return -1;   // DESIGN: -1, none
	const AssistFound found = FindAssistStep(queue, walk, given, own);
	if (found.step >= 0)
		return found.guard;
	for (int32_t i = 0; i < static_cast<int32_t>(queue.size()); ++i) {
		const OrderView& c = queue[i];
		if (!walk.Has(c.tag) && !(IsOwn(own, c) && own.kind == OwnKind::Follow))
			return i;
	}
	return -1;   // DESIGN: -1, none
}

void RecordGiven(std::vector<GivenWork>& given, CommandKind kind, std::span<const float> params, bool shift) {
	if (kind == CommandKind::Insert || kind == CommandKind::Remove)
		return;
	if (IsRepairOrReclaim(kind) && params.size() == 1) {
		if (!shift)
			given.clear();
		if (!WasGiven(given, kind, params[0]))
			given.push_back({kind, params[0]});
		return;
	}
	if (!shift && kind != CommandKind::FireState && kind != CommandKind::MoveState)
		given.clear();
}

Task WorkOf(const OrderView& work, const WorkFacts& facts, int32_t maxUnits) {
	Task task;
	const bool oneParam = work.numParams == 1;
	const int32_t param = (work.numParams == 0) ? NO_UNIT : static_cast<int32_t>(work.params[0]);
	if (work.kind == CommandKind::Guard) {
		task.kind = WorkKind::Guard;
		task.target = param;
		return task;
	}
	if (IsRepairOrReclaim(work.kind) && oneParam) {
		if (work.kind == CommandKind::Reclaim) {
			task.kind = WorkKind::Reclaim;
			task.isFeature = param >= maxUnits;
			task.target = task.isFeature ? param - maxUnits : param;
			return task;
		}
		if (!facts.targetValid)
			return task;
		task.kind = (facts.targetBuild < 1) ? WorkKind::Build : WorkKind::Repair;
		task.target = param;
		return task;
	}
	// a build order with its nanoframe, an area order's unit
	if (facts.building != NO_UNIT) {
		task.kind = (facts.buildingBuild < 1) ? WorkKind::Build : WorkKind::Repair;
		task.target = facts.building;
	}
	return task;
}

Step NextPatrolCheck(Step previous, Step now, Step interval, UnitId unit) {
	// the first check is at once; the one after it falls on the unit's own phase of the interval
	if (previous == NO_STEP)
		return now + 1 + unit % interval;
	return now + interval;
}

FrontClass ClassifyFront(CommandKind orderKind, TurnPath path, bool assistFound, bool engineWorkInFront) {
	if (orderKind == CommandKind::Count)
		return FrontClass::None;
	if (assistFound)
		return FrontClass::Assist;
	if (orderKind == CommandKind::Guard)
		return FrontClass::Guard;
	if (engineWorkInFront)
		return FrontClass::EngineWork;
	const bool fightOrPatrol = orderKind == CommandKind::Fight || orderKind == CommandKind::Patrol;
	if (path == TurnPath::OwnFightOrPatrol || fightOrPatrol)
		return FrontClass::OwnFightOrPatrol;
	return FrontClass::Plain;
}

} // namespace KT1::Core::Orders
