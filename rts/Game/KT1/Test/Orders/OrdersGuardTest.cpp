/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Orders: the patrol, guard, assist and reach decisions (GuardRules.h) and the config's fixture overlay, at 30 and
// at 10 steps per second. The expected values are ord_orders.lua's.

#include <cmath>

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/GuardRules.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"

using namespace KT1::Core::Orders;
using KT1::Core::Rate;

namespace {
	constexpr int STEPS_FAST = KT1::Test::STEP_RATES[0];

	bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }

	// the tests' trig; in the engine the adapter hands in the engine's
	ReachParams TestReach() {
		ReachParams p = MakeReachParams(OrdersConfig{});
		p.trig.cos = [](float a) { return std::cos(a); };
		p.trig.sin = [](float a) { return std::sin(a); };
		p.trig.atan2 = [](float y, float x) { return std::atan2(y, x); };
		return p;
	}
}

KT1_CASE("ORD-GUARD-NEAREST", "the two tie rules of the Lua, and the repair threshold") {
	Nearest low, first;
	for (const UnitId u: {9, 4, 7}) {
		low.OfferByLowestId(u, 10);
		first.OfferFirst(u, 10);
	}
	KT1_CHECK_EQ(low.id, 4);
	KT1_CHECK_EQ(first.id, 9);
	low.OfferByLowestId(12, 9);
	first.OfferFirst(12, 9);
	KT1_CHECK(low.id == 12 && first.id == 12 && low.distance == 9);
	KT1_CHECK(NeedsRepair(75, 100, 0.75f, 0.000001f));
	KT1_CHECK(!NeedsRepair(76, 100, 0.75f, 0.000001f));
	KT1_CHECK(!NeedsRepair(100, 100, 1, 0.000001f));
	KT1_CHECK(WantsShield(100, 100, false, true));
	KT1_CHECK(!WantsShield(99, 100, false, true) && !WantsShield(100, 100, true, true));
	KT1_CHECK(!WantsShield(100, 100, false, false));
}

KT1_RATE_CASE("ORD-GUARD-FOLLOW", "a guard follows where the guarded unit will be in a second, short of it") {
	const GuardParams p = MakeGuardParams(OrdersConfig{});
	const float sps = static_cast<float>(testCase.StepsPerSecond());
	KT1_CHECK_EQ(p.check, testCase.StepsPerSecond() == STEPS_FAST ? 15 : 5);
	KT1_CHECK(p.half == 120 && p.leadSteps == sps && p.behind == 32);
	FollowFacts f;
	f.gx = 1000;
	f.gz = 500;
	f.ux = 1050;
	f.uz = 500;
	KT1_CHECK(!PlanFollow(f, p).move);          // at rest and near
	f.ux = 1100;
	FollowPlan plan = PlanFollow(f, p);         // at rest, farther than half the follow distance
	KT1_CHECK(plan.move && plan.removeOwnFront && Near(plan.x, 1032) && Near(plan.z, 500));
	f.vx = 30 / sps;                            // 30 elmos per second
	f.ux = 1040;
	plan = PlanFollow(f, p);
	KT1_CHECK(plan.move && Near(plan.x, 1040));   // nearer than the stop distance: it stays where it stands
}

KT1_RATE_CASE("ORD-GUARD-RING", "a ringed guard settles at its place and walks back only when pushed away") {
	const GuardParams p = MakeGuardParams(OrdersConfig{});
	FollowFacts f;
	f.hasRing = true;
	f.gx = 100;
	f.ringDx = 40;
	f.ux = 145;
	FollowPlan plan = PlanFollow(f, p);          // within 12 elmos: settled
	KT1_CHECK(plan.ringSettled && !plan.move);
	f.ringSettled = true;
	f.ux = 160;
	plan = PlanFollow(f, p);                     // 20 off, settled: allowed up to 24
	KT1_CHECK(plan.ringSettled && !plan.move);
	f.ux = 170;
	plan = PlanFollow(f, p);
	KT1_CHECK(!plan.ringSettled && plan.move && plan.x == 140);
	f.frontIsMove = true;
	f.frontX = 142;
	KT1_CHECK(!PlanFollow(f, p).move);           // already on its way there
	f.vx = 30 / static_cast<float>(testCase.StepsPerSecond());
	plan = PlanFollow(f, p);
	KT1_CHECK(plan.move && Near(plan.x, 170));
}

KT1_CASE("ORD-GUARD-ASSIST", "the assist step a guarding builder should hold") {
	DefFacts def, reclaimer;
	reclaimer.Set(DefFlag::Reclaims, true);
	const auto want = [&](WorkKind kind, int32_t target, const DefFacts& d, bool feature = false) {
		return WantedAssist(Task{kind, target, feature, NO_UNIT}, 1, 2, d, 1000);
	};
	AssistWant w = want(WorkKind::Build, 5, def);
	KT1_CHECK(w.wanted && w.kind == CommandKind::Repair && w.param == 5);
	KT1_CHECK(!want(WorkKind::Repair, 1, def).wanted);   // itself
	KT1_CHECK(!want(WorkKind::Reclaim, 5, def).wanted);
	w = want(WorkKind::Reclaim, 5, reclaimer, true);
	KT1_CHECK(w.wanted && w.kind == CommandKind::Reclaim && w.param == 1005);
	w = want(WorkKind::Factory, 5, def);
	KT1_CHECK(w.wanted && w.kind == CommandKind::Guard && w.param == 5 && !w.leaveAlone);
	w = want(WorkKind::Factory, 2, def);
	KT1_CHECK(w.leaveAlone && !w.wanted);
	w = want(WorkKind::None, 5, def);
	KT1_CHECK(!w.leaveAlone && !w.wanted);
}

KT1_RATE_CASE("ORD-GUARD-REACH", "a stalled builder looks for a spot after a second, at most the tries") {
	const ReachParams p = TestReach();
	const Step second = testCase.StepsPerSecond();
	ReachStall s;
	KT1_CHECK(ReachCheckDue(0, p.every, false, p) && !ReachCheckDue(1, p.every, false, p));
	KT1_CHECK(ReachCheckDue(1, p.walkingEvery - 1, true, p));
	KT1_CHECK(!NoteReach(s, CommandKind::Repair, 9, 200, 100, 0, p));
	KT1_CHECK(!NoteReach(s, CommandKind::Repair, 9, 199, 100, second - 1, p));
	KT1_CHECK(NoteReach(s, CommandKind::Repair, 9, 199, 100, second, p));
	KT1_CHECK(!NoteReach(s, CommandKind::Repair, 9, 190, 100, second + 1, p) && s.best == 190);
	KT1_CHECK(!NoteReach(s, CommandKind::Repair, 9, 90, 100, 9 * second, p) && s.best == 90);
	s.tries = p.tries;
	KT1_CHECK(!NoteReach(s, CommandKind::Repair, 9, 200, 100, 20 * second, p));
	KT1_CHECK(!NoteReach(s, CommandKind::Reclaim, 9, 200, 100, 20 * second, p) && s.tries == 0);
	NoteSpotSearch(s, 150, 21 * second);
	KT1_CHECK(s.tries == 1 && s.best == 150 && s.since == 21 * second);
	NoteSpot(s, 10, 10);
	KT1_CHECK(SpotTried(s, 20, 10, p) && !SpotTried(s, 30, 10, p));
	for (int i = 0; i < 12; ++i)
		NoteSpot(s, 0, 0);
	KT1_CHECK_EQ(s.tried, static_cast<int32_t>(REACH_TRIED));
}

KT1_RATE_CASE("ORD-GUARD-WALK", "the way round to a spot, its time limit and its end") {
	const ReachParams p = TestReach();
	const float sps = static_cast<float>(testCase.StepsPerSecond());
	KT1_CHECK_EQ(SpotRadius(10, 100, p), 94.0f);
	KT1_CHECK_EQ(SpotRadius(98, 100, p), 102.0f);
	float x = 0, z = 0;
	SpotPoint(6, 100, 100, 50, p, x, z);   // a quarter turn
	KT1_CHECK(Near(x, 100) && Near(z, 150));
	// from east of the target to a spot north of it: back out to 124, two steps of 45 degrees, then in
	const Walk w = PlanWalk(100, 0, 0, 0, 0, 94, 100, 100, p);
	KT1_CHECK_EQ(w.count, 4);
	KT1_CHECK(Near(w.points[0][0], 124) && Near(w.points[0][1], 0));
	KT1_CHECK(Near(w.points[2][0], 0) && Near(w.points[2][1], 124));
	KT1_CHECK(w.points[3][0] == 0 && w.points[3][1] == 94);
	KT1_CHECK(std::fabs(w.length - (24 + 2 * 94.906f + 30)) < 0.01f);
	const Walk back = PlanWalk(100, 0, 0, 0, -94, 0, 100, 100, p);
	KT1_CHECK_EQ(back.count, 6);
	ReachStall s;
	KT1_CHECK(WalkExpired(s, 0));
	s.active = true;
	KT1_CHECK(WalkExpired(s, 0));
	BeginWalk(s, 100, 50, 10, p);
	KT1_CHECK(s.walked && Near(s.walkFor, 6 * sps));   // 100 / 50 * 1.5 + 3 seconds
	KT1_CHECK(!WalkExpired(s, 10 + static_cast<Step>(6 * sps) - 1));
	KT1_CHECK(WalkExpired(s, 10 + static_cast<Step>(6 * sps)));
	BeginWalk(s, 100, 0, 10, p);                       // a speed under 1 counts as 1
	KT1_CHECK(Near(s.walkFor, 153 * sps));
	KT1_CHECK(WalkArrived(98, 0, 0, 0, 100, p) && !WalkArrived(99, 0, 0, 0, 100, p));
	EndWalk(s, 77);
	KT1_CHECK(s.since == 77 && std::isinf(s.best));
	ReachStall none;
	EndWalk(none, 77);
	KT1_CHECK(none.since == NO_STEP);
}

KT1_CASE("ORD-GUARD-FIXTURE", "a fixture overlays values by the Lua's keys on a copy") {
	OrdersConfig base;
	base.fixtures.push_back({"SF", {{"guard_check_interval", 2}, {"guard_radius", 40}, {"queue_min", 25}}});
	const OrdersConfig with = WithFixture(base, "SF");
	KT1_CHECK(with.guard.check_s == 2 && with.guard.half_radius_mu == 20 && with.queue_min == 25);
	KT1_CHECK(base.guard.check_s == 0.5f && WithFixture(base, "none").queue_min == 50);
	KT1_CHECK_EQ(WithFixture(base, "").queue_min, 50);
	OrdersConfig c;
	for (const char* key: {"patrol_check_interval", "patrol_repair_threshold", "load_time", "unload_time", "drop_accuracy",
	                       "selfdestruct_delay", "give_up_time", "fight_check_interval"})
		KT1_CHECK_MSG(SetValue(c, key, 7), key);
	AMoveConfig amove;
	for (const char* key: {"fight_resume_tick", "fight_guard_factor", "fight_stop_share"})
		KT1_CHECK_MSG(SetValue(amove, key, 7), key);
	KT1_CHECK(c.patrol.patrol_check_s == 7 && c.transport.drop_accuracy_mu == 7 && amove.stop_share == 7);
	base.fixtures.push_back({"AM", {{"fight_stop_share", 3}}});
	KT1_CHECK(WithFixture(AMoveConfig{}, base, "AM").stop_share == 3 && WithFixture(AMoveConfig{}, base, "").stop_share == 0.9f);
	KT1_CHECK(!SetValue(c, "no_such", 1));
}
