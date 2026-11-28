/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Orders/QueueHandling, engine-free: ord_orders.lua's waypoint step, cut point, transport clocks, ferry and
// self-destruct (LUA-FREE S12), at 30 and at 10 steps per second where a period is involved.

#include "../KT1Test.h"

#include <cmath>
#include <vector>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Orders/OrdersConfig.h"
#include "Game/KT1/Core/Orders/QueueHandling.h"

using namespace KT1::Core::Orders;
using KT1::Core::Rate;

namespace {

WaypointParams Wp(int sps = 30) {
	Rate::SetStepsPerSecond(sps);
	return MakeWaypointParams(OrdersConfig{});
}

WaypointFacts At(float ux, float uz, float x, float z, float radius, KT1::Core::Orders::Step now) {
	WaypointFacts f;
	f.ux = ux; f.uz = uz;
	f.x = x; f.z = z;
	f.orderedX = x; f.orderedZ = z;
	f.radius = radius;
	f.vx = 1; f.vz = 0;
	f.now = now;
	return f;
}

} // namespace

KT1_RATE_CASE("ORDQ-WP-PARAMS", "the waypoint step's limits in steps at 30 and at 10") {
	const int sps = testCase.StepsPerSecond();
	const WaypointParams p = Wp(sps);
	KT1_CHECK_EQ(p.noProgress, 3.0f * sps);
	KT1_CHECK_EQ(p.restSteps, 1.0f * sps);
	KT1_CHECK(std::fabs(p.restSpeed - 0.3f / sps) < 1e-7f);
	KT1_CHECK_EQ(p.arrive, 1.2f * 8);
}

KT1_CASE("ORDQ-WP-ARRIVE", "MOV-020 R1: within the radius of the ordered point the step is done") {
	const WaypointParams p = Wp();
	WaypointStep w;
	const WaypointAnswer a = RunWaypoint(w, At(5, 0, 0, 0, 9.6f, 10), p);
	KT1_CHECK(a.finished);
	KT1_CHECK(!a.setGoal);
}

KT1_CASE("ORDQ-WP-GOAL", "a new step sets the move goal once: the radius for an advance unit, else a share of it") {
	const WaypointParams p = Wp();
	WaypointStep w;
	WaypointFacts f = At(100, 0, 0, 0, 10, 10);
	f.offset = 2;
	WaypointAnswer a = RunWaypoint(w, f, p);
	KT1_CHECK(a.setGoal && !a.finished);
	KT1_CHECK_EQ(a.goalRadius, (10.0f - 2.0f) * 0.8f);
	f.offset = 9;                                           // the floor: R * 0.4
	w = WaypointStep{};
	KT1_CHECK_EQ(RunWaypoint(w, f, p).goalRadius, 10.0f * 0.4f * 0.8f);
	f.hasAdvance = true;
	w = WaypointStep{};
	KT1_CHECK_EQ(RunWaypoint(w, f, p).goalRadius, 10.0f);
	f.now = 11;
	KT1_CHECK(!RunWaypoint(w, f, p).setGoal);              // the same point: no new goal
}

KT1_CASE("ORDQ-WP-NO-PROGRESS", "no nearer by 0.5 elmo for 3 s near the point (or 6 s anywhere) ends the step") {
	const WaypointParams p = Wp();
	WaypointStep w;
	WaypointFacts f = At(30, 0, 0, 0, 10, 0);               // within four radii
	RunWaypoint(w, f, p);
	f.now = 89;
	KT1_CHECK(!RunWaypoint(w, f, p).finished);
	f.now = 90;
	KT1_CHECK(RunWaypoint(w, f, p).finished);
	KT1_CHECK(!w.active);
	WaypointStep far;
	WaypointFacts g = At(100, 0, 0, 0, 10, 0);              // beyond four radii: twice as long
	RunWaypoint(far, g, p);
	g.now = 90;
	KT1_CHECK(!RunWaypoint(far, g, p).finished);
	g.now = 180;
	KT1_CHECK(RunWaypoint(far, g, p).finished);
}

KT1_CASE("ORDQ-WP-REST", "at rest without getting there: the step ends after one second of calls") {
	const WaypointParams p = Wp();
	WaypointStep w;
	WaypointFacts f = At(100, 0, 0, 0, 10, 0);
	f.vx = 0;
	bool done = false;
	int calls = 0;
	while (!done && calls < 100) { f.now = calls; f.ux = 100 - calls; done = RunWaypoint(w, f, p).finished; ++calls; }
	KT1_CHECK_EQ(calls, 31);   // each call at rest adds one; done once still > 30
}

KT1_CASE("ORDQ-GOALCELL", "records match the stopping point within 1e-3; the oldest goes past the cap") {
	std::vector<GoalCellRecord> recs;
	for (int i = 0; i < 34; ++i) AddGoalCell(recs, GoalCellRecord{static_cast<float>(i), 0, static_cast<float>(i) + 100, 0, 0}, 32);
	KT1_CHECK_EQ(static_cast<int>(recs.size()), 32);
	GoalCellRecord r;
	KT1_CHECK(!FindGoalCell(recs, 0, 0, 1e-3f, r));         // removed
	KT1_CHECK(FindGoalCell(recs, 5.0005f, 0, 1e-3f, r));
	KT1_CHECK_EQ(r.x, 105.0f);
}

KT1_CASE("ORDQ-CUT-POINT", "UnitCmdDone: a cut move gets a step, to the ordered point for advance units") {
	const WaypointParams p = Wp();
	std::vector<GoalCellRecord> recs = {GoalCellRecord{50, 50, 60, 7, 60}};
	MoveDoneFacts f;
	f.ux = 0; f.uz = 0;
	f.x = 50; f.y = 3; f.z = 50;
	f.radius = 9.6f;
	WaypointDue due;
	KT1_CHECK(PlanWaypointStep(f, recs, p, due));
	KT1_CHECK(due.x == 50 && due.y == 3 && due.z == 50);    // the rest point
	f.toOrdered = true;
	due = WaypointDue{};
	KT1_CHECK(PlanWaypointStep(f, recs, p, due));
	KT1_CHECK(due.x == 60 && due.y == 7 && due.z == 60);    // the ordered point
	f.ux = 58; f.uz = 58;                                   // within R of the ordered point: no step
	KT1_CHECK(!PlanWaypointStep(f, recs, p, due));
	f.ux = 0; f.uz = 0;
	f.internal = true;
	KT1_CHECK(!PlanWaypointStep(f, recs, p, due));
	f.internal = false;
	f.ownMove = true;                                       // the fork's move type: only with more moves queued
	KT1_CHECK(!PlanWaypointStep(f, recs, p, due));
	f.moreMoves = true;
	KT1_CHECK(PlanWaypointStep(f, recs, p, due));
}

KT1_RATE_CASE("ORDQ-LOAD-CLOCK", "ORD-007 R1: the load waits load_s from the first ask, half a slow update less from the order") {
	const int sps = testCase.StepsPerSecond();
	Rate::SetStepsPerSecond(sps);
	const TransportParams p = MakeTransportParams(OrdersConfig{});
	KT1_CHECK_EQ(p.askHalf, (sps == 30) ? 7 : 2);
	const float loadS = 2.5f;
	GateClock g;
	const int need = static_cast<int>(std::ceil(loadS * sps - 1e-6f));
	for (int f = 100; f < 100 + need; ++f) KT1_CHECK(!AskLoad(g, 7, f, f, loadS, p));
	KT1_CHECK(AskLoad(g, 7, 100 + need, 100 + need, loadS, p));
	KT1_CHECK(!g.active);
	GateClock h;                                            // from the order: started earlier, half the slow update off
	KT1_CHECK(!AskLoad(h, 7, 200, 190, loadS, p));
	KT1_CHECK(h.fromOrder);
	KT1_CHECK(AskLoad(h, 7, 190 + need - p.askHalf, 190, loadS, p));
}

KT1_CASE("ORDQ-LOAD-GAP", "a gate not asked for over a second, or asked about another unit, starts again") {
	Rate::SetStepsPerSecond(30);
	const TransportParams p = MakeTransportParams(OrdersConfig{});
	GateClock g;
	AskLoad(g, 7, 100, 100, 2.5f, p);
	AskLoad(g, 7, 131, 131, 2.5f, p);                       // 31 steps later: restarted
	KT1_CHECK_EQ(g.start, 131);
	AskLoad(g, 8, 132, 132, 2.5f, p);                       // another unit
	KT1_CHECK_EQ(g.cargo, 8);
	KT1_CHECK_EQ(g.start, 132);
}

KT1_CASE("ORDQ-UNLOAD-INTERVAL", "G0-TRANSPORT-LOAD v3: the first unit waits unload_s, each next one the interval") {
	Rate::SetStepsPerSecond(30);
	const TransportParams p = MakeTransportParams(OrdersConfig{});
	GateClock g;
	int f = 0;
	while (!AskUnload(g, 1, f, f > 0 ? 0 : f, 1.0f, 0.5f, p)) ++f;
	KT1_CHECK_EQ(f, 30);
	KT1_CHECK_EQ(g.setDown, 1);
	const int first = f;
	++f;
	while (!AskUnload(g, 2, f, 0, 1.0f, 0.5f, p)) ++f;      // another unit, same gate
	KT1_CHECK_EQ(f - first, 15);
	KT1_CHECK_EQ(g.setDown, 2);
}

KT1_CASE("ORDQ-HOLD", "ORD-007 R2: the set-down hold lasts drop_hold_s of asking in a row, then lets go") {
	Rate::SetStepsPerSecond(30);
	const TransportParams p = MakeTransportParams(OrdersConfig{});
	HoldClock h;
	KT1_CHECK(Holding(h, 0, p.dropHold, p));
	for (int f = 1; f < 90; ++f) KT1_CHECK(Holding(h, f, p.dropHold, p));   // asked every step
	KT1_CHECK(!Holding(h, 90, p.dropHold, p));
	KT1_CHECK(Holding(h, 200, p.dropHold, p));             // a pause over a second: a new hold
	KT1_CHECK(OffDropPoint(10, 0, 0, 0, 9.6f));
	KT1_CHECK(!OffDropPoint(9, 0, 0, 0, 9.6f));
}

KT1_CASE("ORDQ-ORDER-CLOCK", "UnitCommand: the pickup's clock survives its hand-over; a queued order keeps the old clock") {
	OrderClock c;
	NoteTransportOrder(c, ClockKind::Load, 10, true, false);
	NoteTransportOrder(c, ClockKind::Load, 12, false, false);
	KT1_CHECK(c.given == 10 && c.pickup);
	NoteTransportOrder(c, ClockKind::Unload, 20, false, true);
	KT1_CHECK(c.kind == ClockKind::Load);
	NoteTransportOrder(c, ClockKind::Unload, 21, false, false);
	KT1_CHECK(c.kind == ClockKind::Unload && c.given == 21);
	KT1_CHECK_EQ(OrderStart(c, ClockKind::Unload, 30, true), 21);
	KT1_CHECK_EQ(OrderStart(c, ClockKind::Unload, 30, false), 30);
	KT1_CHECK_EQ(OrderStart(c, ClockKind::Load, 30, true), 30);
}

KT1_CASE("ORDQ-TIMES", "a transport code's own times win; -1 takes the common value") {
	OrdersConfig c;
	TransportCode code;
	code.code = "AIRT";
	code.load_s = 1.5f;
	code.unload_interval_s = 0.25f;
	c.transport.codes.push_back(code);
	DefFacts d;
	d.transportRow = 0;
	const TransportTimes t = TimesFor(c, d);
	KT1_CHECK_EQ(t.loadS, 1.5f);
	KT1_CHECK_EQ(t.unloadS, c.transport.unload_s);
	KT1_CHECK_EQ(t.intervalS, 0.25f);
	KT1_CHECK_EQ(t.dropAccuracy, c.transport.drop_accuracy_mu * 8);
	DefFacts none;
	KT1_CHECK_EQ(TimesFor(c, none).intervalS, -1.0f);
}

KT1_CASE("ORDQ-FERRY-WAIT", "ORD-007 R4: arrivals in order of arrival step, then id; leavers drop out") {
	FerryRoute r;
	const std::vector<UnitId> a = {5, 9};
	NoteWaiting(r, a, 10);
	const std::vector<UnitId> b = {3, 5, 9};
	NoteWaiting(r, b, 12);
	KT1_CHECK(r.waiting == std::vector<UnitId>({5, 9, 3}));
	KT1_CHECK_EQ(r.lastArrival, 12);
	const std::vector<UnitId> c = {3, 9};
	NoteWaiting(r, c, 13);
	KT1_CHECK(r.waiting == std::vector<UnitId>({9, 3}));
}

KT1_RATE_CASE("ORDQ-FERRY-TRIP", "a trip: busy while ordered, unload again with cargo, done when all are set down") {
	Rate::SetStepsPerSecond(testCase.StepsPerSecond());
	const FerryParams p = MakeFerryParams(OrdersConfig{});
	KT1_CHECK_EQ(p.period, testCase.StepsPerSecond() / 2);
	FerryRoute r;
	r.lastArrival = 100;
	KT1_CHECK(!LeaveDue(r, 0, true, 500, p));
	KT1_CHECK(LeaveDue(r, 1, true, 101, p));
	KT1_CHECK(!LeaveDue(r, 1, false, 100 + static_cast<int>(p.leaveDelay) - 1, p));
	KT1_CHECK(LeaveDue(r, 1, false, 100 + static_cast<int>(p.leaveDelay), p));
	const std::vector<UnitId> carried = {4, 6};
	BeginTrip(r, carried, 200);
	KT1_CHECK(CheckTrip(r, false, 2) == TripState::Busy);
	KT1_CHECK(CheckTrip(r, true, 2) == TripState::Unload);
	KT1_CHECK(CheckTrip(r, true, 0) == TripState::Done);
	KT1_CHECK(ReturnNeeded(33, 0, 0, 0, p));
	KT1_CHECK(!ReturnNeeded(32, 0, 0, 0, p));
}

KT1_RATE_CASE("ORDQ-SELFD", "ORD-010 R4: the unit dies the delay later, rounded to a step") {
	Rate::SetStepsPerSecond(testCase.StepsPerSecond());
	KT1_CHECK_EQ(SelfDestructAt(100, 5.0f), 100 + 5 * testCase.StepsPerSecond());
	KT1_CHECK_EQ(SelfDestructAt(0, 0.25f), testCase.StepsPerSecond() == 30 ? 8 : 3);
}
