/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Orders: the busy-unit turn (QueueRules.h, TurnStep.h, the turn list of OrdersState.h), at 30 and at 10 steps per
// second. The expected values are ord_orders.lua's.

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"
#include "Game/KT1/Core/Orders/TurnTrace.h"

using namespace KT1::Core::Orders;
using KT1::Core::Rate;

namespace {
	using K = CommandKind;
	constexpr int32_t UNITS = 64;
	constexpr int STEPS_FAST = KT1::Test::STEP_RATES[0];
	constexpr int32_t SOLDIER = 1;   // a def that assists nobody
	constexpr int32_t BUILDER = 2;   // isBuilderDef
	const TurnParams FAST = {15, 18, 3, 8, 15, 3};

	OrderView E(K kind, uint32_t tag, bool internal = false, int params = 0, float p0 = 0) {
		OrderView v;
		v.kind = kind;
		v.tag = tag;
		v.internal = internal;
		v.numParams = static_cast<uint8_t>(params);
		v.params[0] = p0;
		return v;
	}

	TurnUnit U(UnitId id, std::initializer_list<OrderView> queue, bool assists = false) {
		TurnUnit u;
		u.unit = id;
		u.defId = assists ? BUILDER : SOLDIER;
		const std::vector<OrderView> q(queue);
		SetTurnQueue(u, q);
		u.queueLen = static_cast<int32_t>(q.size());
		u.queueLenAfter = u.queueLen;
		return u;
	}

	std::unique_ptr<State> NewState() {
		auto s = std::make_unique<State>();
		const OrdersConfig config;
		s->Init(UNITS, 4, 1024, 1024, config);
		s->defs[BUILDER].Set(DefFlag::Assists, true);
		return s;
	}

	std::string Join(const std::vector<int32_t>& ids) {
		std::string out;
		for (const int32_t id: ids)
			out += (out.empty() ? "" : " ") + std::to_string(id);
		return out;
	}

	std::string ListOf(const State& s) {
		std::vector<int32_t> ids;
		for (int32_t i = 0; i < s.turn.Size(); ++i)
			ids.push_back(s.turn.At(i));
		return Join(ids);
	}

	Step Next(const State& s, UnitId unit) { return s.nextCheck[s.SlotOf(unit)]; }

	void Walking(TurnUnit& u, uint32_t tag) {
		u.walking = true;
		u.walk.tags[u.walk.count++] = tag;
	}

	TurnFront Classify(const TurnUnit& u) { return ClassifyUnit(u, u.defId == BUILDER); }

	struct Row {
		const char* name;
		TurnUnit unit;
		TurnPath path;
		FrontClass cls;
		K order;
		int32_t index;
	};

	void CheckRow(KT1::Test::Case& testCase, const Row& row) {
		const TurnFront f = Classify(row.unit);
		const std::string at = std::string("classify: ") + row.name;
		KT1_CHECK_MSG(f.path == row.path, at + " path");
		KT1_CHECK_MSG(f.frontClass == row.cls, at + " class");
		KT1_CHECK_MSG(f.order == row.order, at + " order");
		KT1_CHECK_MSG(f.orderIndex == row.index, at + " index");
	}
}

KT1_CASE("ORD-TURN-ENGINE-WORK", "the engine's work, what counts internal, and the order within the entries read") {
	for (const K kind: {K::Attack, K::Repair, K::Reclaim, K::Capture, K::Resurrect, K::Guard})
		KT1_CHECK(IsEngineWork(kind));
	for (const K kind: {K::Move, K::Fight, K::Patrol, K::AreaAttack, K::ManualFire, K::AMove, K::Build})
		KT1_CHECK(!IsEngineWork(kind));
	const std::vector<OrderView> q = {E(K::Attack, 1), E(K::Fight, 2, true), E(K::Patrol, 3)};
	KT1_CHECK(EngineAhead(q, 0));
	KT1_CHECK(!EngineAhead(q, 1));
	KT1_CHECK(!EngineAhead(q, 2));
	KT1_CHECK(CountsInternal(q, 0) && CountsInternal(q, 1) && !CountsInternal(q, 2));
	KT1_CHECK_EQ(OrderIndex(q), 2);
	// work ahead of a fight that is not internal is an order
	const std::vector<OrderView> own = {E(K::Attack, 1), E(K::Fight, 2)};
	KT1_CHECK_EQ(OrderIndex(own), 0);
	// the eighth entry has nothing behind it, so engine work there is the order; an order past it is not found
	std::vector<OrderView> deep(7, E(K::Move, 9, true));
	deep.push_back(E(K::Repair, 10));
	deep.push_back(E(K::Fight, 11, true));
	KT1_CHECK_EQ(OrderIndex(deep), 7);
	deep[7] = E(K::Move, 10, true);
	deep[8] = E(K::Patrol, 11);
	KT1_CHECK_EQ(OrderIndex(deep), -1);
	KT1_CHECK_EQ(OrderIndex({}), -1);
}

KT1_CASE("ORD-TURN-PATH", "the front entry alone picks the path, and a new attack-move starts its task") {
	DefFacts plain, builder;
	builder.Set(DefFlag::Assists, true);
	KT1_CHECK(PathOf(E(K::Fight, 1), builder) == TurnPath::OwnFightOrPatrol);
	KT1_CHECK(PathOf(E(K::Patrol, 1), plain) == TurnPath::OwnFightOrPatrol);
	KT1_CHECK(PathOf(E(K::Patrol, 1, true), plain) == TurnPath::ReadQueue);
	KT1_CHECK(PathOf(E(K::Move, 1), plain) == TurnPath::Plain);
	KT1_CHECK(PathOf(E(K::AMove, 1), plain) == TurnPath::Plain);
	KT1_CHECK(PathOf(E(K::Move, 1), builder) == TurnPath::ReadQueue);
	KT1_CHECK(PathOf(E(K::Move, 1, true), plain) == TurnPath::ReadQueue);
	KT1_CHECK(PathOf(E(K::Guard, 1), plain) == TurnPath::ReadQueue);
	KT1_CHECK(PathOf(E(K::Attack, 1), plain) == TurnPath::ReadQueue);
	KT1_CHECK(StartsAMove(E(K::AMove, 5), false, 0));
	KT1_CHECK(StartsAMove(E(K::AMove, 5), true, 4));
	KT1_CHECK(StartsAMove(E(K::AMove, 5, true), false, 0));
	KT1_CHECK(!StartsAMove(E(K::AMove, 5), true, 5));
	KT1_CHECK(!StartsAMove(E(K::Fight, 5), false, 0));
}

KT1_CASE("ORD-TURN-CLASSIFY", "the classification table: every way a unit's turn finds its order") {
	const auto P = TurnPath::ReadQueue;
	TurnUnit walked = U(1, {E(K::Move, 1, true), E(K::Repair, 2, true, 1, 9), E(K::Guard, 3, false, 1, 8)}, true);
	Walking(walked, 1);
	TurnUnit given = U(1, {E(K::Repair, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)}, true);
	given.given = {{K::Repair, 9}};
	TurnUnit ownAttack = U(1, {E(K::Attack, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)}, true);
	ownAttack.own = {2, OwnKind::Attack, 9};
	TurnUnit ownAssist = U(1, {E(K::Repair, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)}, true);
	ownAssist.own = {2, OwnKind::Assist, 9};
	TurnUnit ownStep = U(1, {E(K::Attack, 2, true, 1, 9), E(K::Patrol, 3)});
	ownStep.own = {2, OwnKind::Attack, 9};
	TurnUnit idle = U(1, {});
	TurnUnit emptied = U(1, {E(K::Move, 1)});
	emptied.queueLen = 0;
	const Row rows[] = {
		{"no queue", idle, P, FrontClass::None, NO_ORDER, -1},
		{"queue length 0", emptied, P, FrontClass::None, NO_ORDER, -1},
		{"own fight", U(1, {E(K::Fight, 1)}), TurnPath::OwnFightOrPatrol, FrontClass::OwnFightOrPatrol, K::Fight, 0},
		{"own patrol", U(1, {E(K::Patrol, 1)}, true), TurnPath::OwnFightOrPatrol, FrontClass::OwnFightOrPatrol,
		 K::Patrol, 0},
		{"plain move", U(1, {E(K::Move, 1)}), TurnPath::Plain, FrontClass::Plain, K::Move, 0},
		{"attack-move", U(1, {E(K::AMove, 1)}), TurnPath::Plain, FrontClass::Plain, K::AMove, 0},
		{"builder move", U(1, {E(K::Move, 1)}, true), P, FrontClass::Plain, K::Move, 0},
		{"builder's own repair", U(1, {E(K::Repair, 1, false, 1, 9)}, true), P, FrontClass::Plain, K::Repair, 0},
		{"work ahead of patrol", U(1, {E(K::Attack, 1), E(K::Fight, 2, true), E(K::Patrol, 3)}), P,
		 FrontClass::EngineWork, K::Patrol, 2},
		{"work ahead of fight", U(1, {E(K::Reclaim, 1, true, 1, 9), E(K::Fight, 2)}, true), P, FrontClass::EngineWork,
		 K::Fight, 1},
		{"fight step ahead of patrol", U(1, {E(K::Fight, 1, true), E(K::Patrol, 2)}), P, FrontClass::OwnFightOrPatrol,
		 K::Patrol, 1},
		{"own step ahead of patrol", ownStep, P, FrontClass::OwnFightOrPatrol, K::Patrol, 1},
		{"work ahead of a move", U(1, {E(K::Repair, 1, true, 1, 9), E(K::Move, 2)}, true), P, FrontClass::EngineWork,
		 K::Move, 1},
		{"guard", U(1, {E(K::Guard, 1, false, 1, 8)}), P, FrontClass::Guard, K::Guard, 0},
		{"internal step ahead of guard", U(1, {E(K::Move, 1, true), E(K::Guard, 2, false, 1, 8)}), P, FrontClass::Guard,
		 K::Guard, 1},
		{"engine's pushed repair", U(1, {E(K::Repair, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)}, true), P,
		 FrontClass::Assist, K::Guard, 1},
		{"own assist step", ownAssist, P, FrontClass::Assist, K::Guard, 1},
		{"given repair", given, P, FrontClass::Plain, K::Repair, 0},
		{"own attack, not internal", ownAttack, P, FrontClass::Plain, K::Attack, 0},
		{"walk step before the assist", walked, P, FrontClass::Guard, K::Guard, 2},
		{"only internal entries", U(1, {E(K::Move, 1, true), E(K::Attack, 2, true)}), P, FrontClass::None, NO_ORDER, -1},
	};
	for (const Row& row: rows)
		CheckRow(testCase, row);
	// a non-builder with an engine-pushed repair ahead of a guard still reads as assisting: the Lua asks no def here
	KT1_CHECK(Classify(U(1, {E(K::Repair, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)})).assistFound);
}

KT1_CASE("ORD-TURN-ASSIST", "the assist step of a guard and the lead's work") {
	ReachWalk walk;
	walk.tags[walk.count++] = 1;
	const std::vector<OrderView> q = {E(K::Move, 1, true), E(K::Reclaim, 2, true, 1, 70), E(K::Guard, 3, false, 1, 8)};
	AssistFound f = FindAssistStep(q, walk, {}, {});
	KT1_CHECK_EQ(f.step, 1);
	KT1_CHECK_EQ(f.guard, 2);
	KT1_CHECK_EQ(LeadWorkIndex(q, walk, {}, {}), 2);
	// without the walk the move is something else in front
	f = FindAssistStep(q, {}, {}, {});
	KT1_CHECK(f.step == -1 && f.guard == -1);
	KT1_CHECK_EQ(LeadWorkIndex(q, {}, {}, {}), 0);
	const std::vector<OrderView> front = {E(K::Guard, 3, false, 1, 8)};
	f = FindAssistStep(front, {}, {}, {});
	KT1_CHECK(f.step == -1 && f.guard == 0);
	// a given repair is the player's; internal, it is a step whatever was given
	const std::vector<GivenWork> given = {{K::Repair, 9}};
	std::vector<OrderView> r = {E(K::Repair, 2, false, 1, 9), E(K::Guard, 3, false, 1, 8)};
	KT1_CHECK_EQ(FindAssistStep(r, {}, given, {}).step, -1);
	r[0].internal = true;
	KT1_CHECK_EQ(FindAssistStep(r, {}, given, {}).step, 0);
	KT1_CHECK_EQ(FindAssistStep(r, {}, {}, {2, OwnKind::Repair, 9}).step, -1);
	r[0].numParams = 4;
	KT1_CHECK_EQ(FindAssistStep(r, {}, {}, {}).step, -1);
	// a follow step of its own is passed over for the lead's work
	const std::vector<OrderView> follow = {E(K::Move, 4, true), E(K::Guard, 5, false, 1, 8)};
	KT1_CHECK_EQ(LeadWorkIndex(follow, {}, {}, {4, OwnKind::Follow, 8}), 1);
	KT1_CHECK_EQ(LeadWorkIndex({}, {}, {}, {}), -1);
}

KT1_CASE("ORD-TURN-STRIP", "only forbidden engine work ahead of the order is removed") {
	const std::vector<OrderView> q = {E(K::Attack, 1, true, 1, 9), E(K::Repair, 2, true, 1, 9), E(K::Reclaim, 3),
	                                  E(K::Fight, 4, true), E(K::Move, 5, true), E(K::Patrol, 6)};
	KT1_CHECK(RemovesEngineWork(q, 0, {}, true, false));      // an unarmed target
	KT1_CHECK(!RemovesEngineWork(q, 0, {}, true, true));      // an armed one stays
	KT1_CHECK(!RemovesEngineWork(q, 0, {}, false, false));    // not one valid unit
	KT1_CHECK(RemovesEngineWork(q, 1, {}, true, true));
	KT1_CHECK(!RemovesEngineWork(q, 1, {2, OwnKind::Repair, 9}, true, true));
	KT1_CHECK(RemovesEngineWork(q, 2, {}, false, false));     // ahead of an internal fight
	KT1_CHECK(!RemovesEngineWork(q, 3, {}, false, false));    // the fight step stays
	KT1_CHECK(!RemovesEngineWork(q, 4, {}, false, false));
}

KT1_CASE("ORD-TURN-GIVEN", "the repair and reclaim orders given to a unit are remembered as the Lua does") {
	std::vector<GivenWork> g;
	const float a[] = {9};
	const float b[] = {12};
	const float area[] = {1, 2, 3, 4};
	RecordGiven(g, K::Repair, a, false);
	RecordGiven(g, K::Reclaim, b, true);
	RecordGiven(g, K::Reclaim, b, true);
	KT1_CHECK_EQ(g.size(), size_t(2));
	KT1_CHECK(WasGiven(g, K::Repair, 9) && WasGiven(g, K::Reclaim, 12) && !WasGiven(g, K::Reclaim, 9));
	for (const K keeps: {K::Insert, K::Remove, K::FireState, K::MoveState})
		RecordGiven(g, keeps, a, false);
	RecordGiven(g, K::Move, a, true);
	RecordGiven(g, K::Repair, area, true);
	KT1_CHECK_EQ(g.size(), size_t(2));
	RecordGiven(g, K::Repair, b, false);
	KT1_CHECK(g.size() == 1 && WasGiven(g, K::Repair, 12));
	RecordGiven(g, K::GameFireState, a, false);   // the game's own fire order is not the engine's: it forgets
	KT1_CHECK(g.empty());
}

KT1_CASE("ORD-TURN-WORKOF", "the work of the entry a lead is on") {
	constexpr int32_t MAX = 1000;
	WorkFacts f;
	Task t = WorkOf(E(K::Guard, 1, false, 1, 8), f, MAX);
	KT1_CHECK(t.kind == WorkKind::Guard && t.target == 8);
	t = WorkOf(E(K::Reclaim, 1, false, 1, 1007), f, MAX);
	KT1_CHECK(t.kind == WorkKind::Reclaim && t.target == 7 && t.isFeature);
	t = WorkOf(E(K::Reclaim, 1, false, 1, 7), f, MAX);
	KT1_CHECK(t.kind == WorkKind::Reclaim && t.target == 7 && !t.isFeature);
	t = WorkOf(E(K::Repair, 1, false, 1, 7), f, MAX);
	KT1_CHECK(t.kind == WorkKind::None);
	f.targetValid = true;
	KT1_CHECK(WorkOf(E(K::Repair, 1, false, 1, 7), f, MAX).kind == WorkKind::Repair);
	f.targetBuild = 0.5f;
	KT1_CHECK(WorkOf(E(K::Repair, 1, false, 1, 7), f, MAX).kind == WorkKind::Build);
	KT1_CHECK(WorkOf(E(K::Move, 1, false, 3), f, MAX).kind == WorkKind::None);
	f.building = 33;
	t = WorkOf(E(K::Repair, 1, false, 4), f, MAX);   // an area repair: what the lead builds now
	KT1_CHECK(t.kind == WorkKind::Repair && t.target == 33);
	f.buildingBuild = 0.25f;
	KT1_CHECK(WorkOf(E(K::Build, 1, false, 3), f, MAX).kind == WorkKind::Build);
}

KT1_CASE("ORD-TURN-LIST", "units join at the end and a leaver's slot takes the last") {
	auto s = NewState();
	for (const UnitId u: {5, 2, 9, 7})
		KT1_CHECK(s->turn.Add(u));
	KT1_CHECK(!s->turn.Add(9));
	KT1_CHECK(!s->turn.Add(-1) && !s->turn.Add(UNITS));
	KT1_CHECK_EQ(ListOf(*s), "5 2 9 7");
	KT1_CHECK(s->turn.Remove(2));
	KT1_CHECK_EQ(ListOf(*s), "5 7 9");
	KT1_CHECK(!s->turn.Remove(2) && !s->turn.Has(2) && s->turn.Has(7));
	KT1_CHECK(s->turn.Remove(9));   // the last leaves
	KT1_CHECK_EQ(ListOf(*s), "5 7");
	s->turn.Add(2);
	s->turn.Remove(5);
	KT1_CHECK_EQ(ListOf(*s), "2 7");
	s->turn.Remove(2);
	s->turn.Remove(7);
	KT1_CHECK_EQ(s->turn.Size(), 0);
	KT1_CHECK(s->turn.Add(7));
}

KT1_CASE("ORD-TURN-STATE", "tracking blanks a unit's columns and forgetting drops all of them") {
	auto s = NewState();
	const auto slot = s->Track(7, 2);
	s->Track(8, 3);
	KT1_CHECK_EQ(s->Track(7, 4), slot);
	KT1_CHECK_EQ(s->defId[slot], 2);
	KT1_CHECK(s->DefOf(7) == &s->defs[2] && s->Def(0) == nullptr && s->Def(5) == nullptr && s->DefOf(9) == nullptr);
	KT1_CHECK_EQ(Next(*s, 7), NO_STEP);
	KT1_CHECK(s->guardRepair[slot] == NO_UNIT && s->selfDestruct[slot] == NO_STEP && s->fresh[slot] == NO_STEP);
	KT1_CHECK_EQ(int(s->fireState[slot]), int(FireState::Free));
	s->nextCheck[slot] = 40;
	s->walk[slot].count = 1;
	s->walk[slot].tags[0] = 6;
	KT1_CHECK(s->walk[slot].Has(6) && !s->walk[slot].Has(0));
	s->turn.Add(7);
	s->moveUnits.Insert(7);
	s->nextCheck[s->SlotOf(8)] = 50;
	s->Forget(7);
	KT1_CHECK(!s->turn.Has(7) && !s->moveUnits.Contains(7) && s->DefOf(7) == nullptr);
	KT1_CHECK_EQ(Next(*s, 8), 50);   // the other unit's columns moved with it
	s->Track(7, 2);
	KT1_CHECK(Next(*s, 7) == NO_STEP && s->walk[s->SlotOf(7)].count == 0);
	s->Forget(63);   // a unit never tracked
}

KT1_RATE_CASE("ORD-TURN-PERIODS", "the walk's periods in steps, as H.Every and floor(s * FPS + 0.5) give them") {
	const bool fast = testCase.StepsPerSecond() == STEPS_FAST;
	const TurnParams p = MakeTurnParams(OrdersConfig{});
	KT1_CHECK_EQ(p.guardCheck, fast ? 15 : 5);
	KT1_CHECK_EQ(p.patrolCheck, fast ? 18 : 6);
	KT1_CHECK_EQ(p.fightCheck, fast ? 3 : 1);
	KT1_CHECK_EQ(p.safetyNet, fast ? 8 : 3);
	KT1_CHECK_EQ(p.reachEvery, fast ? 15 : 5);
	KT1_CHECK_EQ(p.reachWalkingEvery, fast ? 3 : 1);
}

KT1_RATE_CASE("ORD-TURN-NEXTCHECK", "the first check at once, then the unit's own phase, then the interval") {
	const TurnParams p = MakeTurnParams(OrdersConfig{});
	KT1_CHECK(CheckDue(NO_STEP, 0) && CheckDue(10, 10) && CheckDue(9, 10) && !CheckDue(11, 10));
	KT1_CHECK_EQ(NextPatrolCheck(NO_STEP, 100, p.patrolCheck, 23), 101 + 23 % p.patrolCheck);
	KT1_CHECK_EQ(NextPatrolCheck(101, 120, p.patrolCheck, 23), 120 + p.patrolCheck);
	for (const K order: {K::Patrol, K::Fight}) {
		auto s = NewState();
		const Step every = order == K::Patrol ? p.patrolCheck : p.fightCheck;
		const TurnUnit u = U(23, {E(order, 1)});
		std::vector<Step> checks;
		int patrols = 0;
		for (Step f = 100; f < 100 + 4 * every + 2; ++f) {
			const Step before = f == 100 ? NO_STEP : Next(*s, 23);
			const TurnWork w = VisitUnit(*s, p, f, u);
			if (Next(*s, 23) != before)
				checks.push_back(f);
			patrols += w.patrolCheck ? 1 : 0;
		}
		KT1_CHECK(checks.size() >= 4);
		KT1_CHECK_EQ(checks[0], 100);
		KT1_CHECK_EQ(checks[1], 101 + 23 % every);
		KT1_CHECK_EQ(checks[2] - checks[1], every);
		KT1_CHECK_EQ(patrols, order == K::Patrol ? static_cast<int>(checks.size()) : 0);
	}
}

KT1_RATE_CASE("ORD-TURN-GUARD-STEP", "a guard is checked every interval and an assisting one looked at between") {
	const TurnParams p = MakeTurnParams(OrdersConfig{});
	auto s = NewState();
	TurnUnit u = U(4, {E(K::Guard, 7, false, 1, 8)}, true);
	TurnWork w = VisitUnit(*s, p, 50, u);
	KT1_CHECK(w.guard == GuardAction::Check && w.guarded == 8.0f && w.orderTag == 7 && w.stripBelow == 0);
	KT1_CHECK_EQ(Next(*s, 4), 50 + p.guardCheck);   // no phase of its own
	w = VisitUnit(*s, p, 51, u);
	KT1_CHECK(w.guard == GuardAction::None);
	u.own = {3, OwnKind::Assist, 9};
	KT1_CHECK(VisitUnit(*s, p, 51, u).guard == GuardAction::Assist);
	KT1_CHECK(VisitUnit(*s, p, 50 + p.guardCheck, u).guard == GuardAction::Check);
	KT1_CHECK_EQ(Next(*s, 4), 50 + 2 * p.guardCheck);
	// guard to patrol keeps the clock; any other order clears it
	VisitUnit(*s, p, 52 + p.guardCheck, U(4, {E(K::Patrol, 9)}));
	KT1_CHECK_EQ(Next(*s, 4), 50 + 2 * p.guardCheck);
	VisitUnit(*s, p, 53 + p.guardCheck, U(4, {E(K::Move, 10)}));
	KT1_CHECK_EQ(Next(*s, 4), NO_STEP);
	// a guard order with no parameter is no guard
	w = VisitUnit(*s, p, 60, U(4, {E(K::Guard, 11)}));
	KT1_CHECK(w.guard == GuardAction::None && Next(*s, 4) == NO_STEP);
	KT1_CHECK_EQ(int(s->frontClass[s->SlotOf(4)]), int(FrontClass::Guard));
}

KT1_RATE_CASE("ORD-TURN-SELECT", "what each turn does: attack-move start, reach work, strip") {
	const TurnParams p = MakeTurnParams(OrdersConfig{});
	auto s = NewState();
	const Step due = p.reachEvery * 10 - 5;   // (f + u) % reachEvery == 0 for unit 5
	TurnWork w = VisitUnit(*s, p, due, U(5, {E(K::Move, 1)}));
	KT1_CHECK(w.plain && !w.Acts());
	w = VisitUnit(*s, p, due, U(5, {E(K::AMove, 2)}));
	KT1_CHECK(w.startAMove && !w.plain && w.reach == ReachAction::None);
	TurnUnit running = U(5, {E(K::AMove, 2)});
	running.hasTask = true;
	running.taskTag = 2;
	KT1_CHECK(!VisitUnit(*s, p, due, running).Acts());
	TurnUnit repair = U(5, {E(K::Repair, 3, false, 1, 9)}, true);
	KT1_CHECK(VisitUnit(*s, p, due, repair).reach == ReachAction::CheckReach);
	KT1_CHECK(VisitUnit(*s, p, due + 1, repair).reach == ReachAction::None);
	Walking(repair, 40);
	KT1_CHECK(VisitUnit(*s, p, due, repair).reach == ReachAction::CheckWalk);
	KT1_CHECK(VisitUnit(*s, p, due + p.reachWalkingEvery, repair).reach == ReachAction::CheckWalk);
	KT1_CHECK(VisitUnit(*s, p, due, U(5, {E(K::Reclaim, 3, false, 4)}, true)).reach == ReachAction::ForgetStall);
	KT1_CHECK(VisitUnit(*s, p, due, U(5, {E(K::Patrol, 3)}, true)).reach == ReachAction::ForgetStall);
	KT1_CHECK(VisitUnit(*s, p, due, U(5, {E(K::Patrol, 3)})).reach == ReachAction::None);
	w = VisitUnit(*s, p, due, U(5, {E(K::Attack, 1, true), E(K::Fight, 2, true), E(K::Patrol, 3)}));
	KT1_CHECK_EQ(w.stripBelow, 2);
	KT1_CHECK_EQ(VisitUnit(*s, p, due, U(5, {E(K::Patrol, 3)})).stripBelow, 0);
}

KT1_CASE("ORD-TURN-WALK", "units leave on an empty queue and the slot's new unit is visited in the same walk") {
	auto s = NewState();
	TurnParams params = FAST;
	TurnInput in;
	in.now = 1;
	for (const UnitId u: {6, 3, 8, 1})
		in.events.push_back({TurnEventKind::Join, u});
	TurnUnit three = U(3, {});
	TurnUnit one = U(1, {E(K::Patrol, 1)});
	TurnUnit eight = U(8, {E(K::Fight, 1)});
	eight.queueLenAfter = 0;                       // its turn emptied the queue
	TurnUnit six = U(6, {E(K::Move, 1)});
	six.queueLenAfter = 0;                         // plain: the length before the turn holds
	in.units = {one, three, six, eight};
	TurnOutput out;
	RunTurnStep(*s, params, in, out);
	KT1_CHECK_EQ(Join(out.turnList), "6 3 1 8");   // 3 leaves, 1 takes its slot; 8 leaves last
	KT1_CHECK_EQ(Join(out.listAfter), "6 1");
	KT1_CHECK_EQ(Join(s->turnThisStep), "6 3 1 8");
	KT1_CHECK(out.nextCheck[1] == NO_STEP && out.nextCheck[3] == NO_STEP);
	KT1_CHECK_EQ(out.nextCheck[2], 1 + 1 + 1 % 18);
	KT1_CHECK(out.missing.empty());
	// an order brings 3 back at the end; a destroyed 6 gives its slot to the last
	in.now = 2;
	in.events = {{TurnEventKind::Join, 3}, {TurnEventKind::Leave, 6}};
	in.units = {one, U(3, {E(K::Move, 2)})};
	RunTurnStep(*s, params, in, out);
	KT1_CHECK_EQ(Join(out.turnList), "3 1");
	KT1_CHECK_EQ(Join(out.listAfter), "3 1");
}

KT1_CASE("ORD-TURN-GONE", "a unit destroyed during its turn leaves its slot to be visited again") {
	auto s = NewState();
	TurnParams params = FAST;
	TurnInput in;
	in.now = 1;
	for (const UnitId u: {2, 4, 6})
		in.events.push_back({TurnEventKind::Join, u});
	TurnUnit two = U(2, {E(K::Patrol, 1)});
	two.gone = true;
	in.units = {two, U(4, {E(K::Move, 1)}), U(6, {E(K::Move, 1)})};
	TurnOutput out;
	RunTurnStep(*s, params, in, out);
	KT1_CHECK_EQ(Join(out.turnList), "2 6 4");
	KT1_CHECK_EQ(Join(out.listAfter), "6 4");
	KT1_CHECK(out.nextCheck[0] == NO_STEP && s->SlotOf(2) == KT1::Core::DenseIndex::NONE);
	// a unit with no record is reported and read as idle
	in.events.clear();
	in.units = {U(4, {E(K::Move, 1)})};
	RunTurnStep(*s, params, in, out);
	KT1_CHECK_EQ(Join(out.missing), "6");
	KT1_CHECK_EQ(Join(out.listAfter), "4");
}

KT1_RATE_CASE("ORD-TURN-SCENARIO", "several steps: safety net, joins in net order, phases and a replay equal itself") {
	TurnParams p = MakeTurnParams(OrdersConfig{});
	const Step net = p.safetyNet;
	auto a = NewState();
	auto b = NewState();
	TurnOutput outA, outB;
	TurnInput in;
	in.netUnits = {11, 12, 13};
	const TurnUnit patrol = U(12, {E(K::Patrol, 1)});
	const TurnUnit guard = U(13, {E(K::Guard, 2, false, 1, 11)});
	for (Step f = net - 1; f <= net * 3; ++f) {
		in.now = f;
		in.events.clear();
		if (f == net + 1)
			in.events = {{TurnEventKind::Leave, 11}};
		TurnUnit move = U(11, {E(K::Move, 3)});
		in.units = {move, patrol, guard};
		RunTurnStep(*a, p, in, outA);
		RunTurnStep(*b, p, in, outB);
		KT1_CHECK(outA.turnList == outB.turnList && outA.nextCheck == outB.nextCheck);
		KT1_CHECK(outA.frontClass == outB.frontClass && outA.listAfter == outB.listAfter && outA.missing.empty());
		if (f == net - 1)
			KT1_CHECK(outA.turnList.empty());
		if (f == net) {
			KT1_CHECK_EQ(Join(outA.turnList), "11 12 13");
			KT1_CHECK_EQ(Next(*a, 12), net + 1 + 12 % p.patrolCheck);
			KT1_CHECK_EQ(Next(*a, 13), net + p.guardCheck);
		}
		if (f == net + 1)   // 13 took 11's slot, and the next net step has not come
			KT1_CHECK_EQ(Join(outA.turnList), "13 12");
		if (f == net * 2)   // 11 still has a queue in the net's list: it joins again, at the end
			KT1_CHECK_EQ(Join(outA.turnList), "13 12 11");
	}
}

namespace {
	// a live port whose first turn gives another unit an order
	struct JoiningPort {
		State& state;
		TurnUnit facts;
		std::vector<int32_t> seen;
		const TurnUnit& Read(UnitId u) { facts = U(u, {E(K::Move, 1)}); return facts; }
		void Act(const TurnUnit& f, const TurnWork&) { if (f.unit == 3) state.turn.Add(2); }
		int32_t QueueLenAfter(const TurnUnit& f) { return f.queueLen; }
		void Done(const TurnWork& w) { seen.push_back(w.unit); }
	};
}

KT1_CASE("ORD-TURN-JOIN-IN-WALK", "a unit that joins during the walk is visited at its end") {
	auto s = NewState();
	s->turn.Add(3);
	s->turn.Add(5);
	JoiningPort port{*s, {}, {}};
	WalkTurns(*s, FAST, 1, port);
	KT1_CHECK_EQ(Join(port.seen), "3 5 2");
}

KT1_CASE("ORD-TURN-WALK-EVENTS", "recorded joins and leaves of a turn apply during the walk; a second visit has its own record") {
	auto s = NewState();
	TurnParams params = FAST;
	TurnInput in;
	in.now = 1;
	for (const UnitId u: {5, 3, 7})
		in.events.push_back({TurnEventKind::Join, u});
	TurnUnit first = U(3, {});                                 // visited idle: it leaves
	TurnUnit again = U(3, {E(K::Move, 4)});                    // and an order given in 9's turn brings it back
	TurnUnit five = U(5, {E(K::Patrol, 1)});
	five.during = {{TurnEventKind::Leave, 7}, {TurnEventKind::Join, 9}};
	TurnUnit nine = U(9, {E(K::Move, 2)});
	nine.during = {{TurnEventKind::Join, 3}};
	in.units = {first, again, five, nine};
	// the record goes through the trace's bytes on the way
	KT1::Core::Trace::Bytes bytes;
	Put(bytes, in);
	KT1::Core::Trace::ByteReader reader(bytes.View());
	TurnInput read;
	KT1_CHECK(Get(reader, read));
	TurnOutput out;
	RunTurnStep(*s, params, read, out);
	// 5's turn destroys 7 before its slot is reached and brings in 9; 3 leaves, 9 takes its slot and brings 3 back
	KT1_CHECK_EQ(Join(out.turnList), "5 3 9 3");
	KT1_CHECK_EQ(Join(out.listAfter), "5 9 3");
	KT1_CHECK(out.missing.empty());
	KT1_CHECK_EQ(int(out.frontClass[1]), int(FrontClass::None));
	KT1_CHECK_EQ(int(out.frontClass[3]), int(FrontClass::Plain));
}

KT1_CASE("ORD-TURN-QUEUE-FILL", "only the first eight entries of a queue are kept") {
	std::vector<OrderView> q(10, E(K::Move, 50));
	q[7] = E(K::Patrol, 7);
	TurnUnit u;
	SetTurnQueue(u, q);
	KT1_CHECK(u.numQueue == QUEUE_READ && u.queue[7].tag == 7);
	SetTurnQueue(u, {});
	KT1_CHECK(u.numQueue == 0 && u.queue[7].tag == 0);
}
