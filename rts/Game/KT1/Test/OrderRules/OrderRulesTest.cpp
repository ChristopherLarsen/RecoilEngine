/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core cases (LUA-FREE S11, S14): the row names, the unit classes, each guard test and the guard state.

#include <array>
#include <string>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/OrderRules/GuardLogic.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::OrderRules;
using KT1::Core::DenseIndex;
using KT1::Core::Rate;
using KT1::Core::Orders::AcceptWord;
using KT1::Core::Orders::BitOf;
using KT1::Core::Orders::CommandKind;
using KT1::Core::Orders::DefFacts;
using KT1::Core::Orders::DefFlag;
using KT1::Core::Orders::FireState;
using KT1::Core::Orders::MaskOf;
using KT1::Core::Orders::OrderView;

namespace {

DefFacts Tank() {
	DefFacts d;
	d.Set(DefFlag::CanMove, true);
	d.speed = 60;
	return d;
}

OrderView MoveTo(float x, float z, uint8_t params = 3) {
	OrderView v;
	v.kind = CommandKind::Move;
	v.numParams = params;
	v.params = {x, 0, z};
	return v;
}

} // namespace

KT1_CASE("ORDR-NAMES", "every row name parses to its enum and an unknown one is refused") {
	Condition c = Condition::Count;
	KT1_CHECK(ParseCondition("always", c) && c == Condition::Always);
	KT1_CHECK(ParseCondition("reclaim_while_moving", c) && c == Condition::ReclaimWhileMoving);
	KT1_CHECK(ParseCondition("indirect_known_target", c) && c == Condition::IndirectKnownTarget);
	KT1_CHECK(!ParseCondition("Always", c));
	UnitClass u = UnitClass::Count;
	KT1_CHECK(ParseUnitClass("idle_lander", u) && u == UnitClass::IdleLander);
	KT1_CHECK(ParseUnitClass("ground_not_command", u) && u == UnitClass::GroundNotCommand);
	Action a = Action::Count;
	KT1_CHECK(ParseAction("refuse_and_reissue", a) && a == Action::Reissue);
	KT1_CHECK(!ParseAction("reissue", a));
	CallIn in = CallIn::Count;
	KT1_CHECK(ParseCallIn("transport_unload", in) && in == CallIn::TransportUnload);
	KT1_CHECK(!ParseCallIn("", in));
}

KT1_CASE("ORDR-COMPILE", "a row compiles with its source index; an unknown name fails and leaves the output alone") {
	OrderRuleRow row;
	row.condition = "beyond_map";
	row.unit_class = "not_air";
	row.internal_too = true;
	CompiledRow out;
	KT1_CHECK(Compile(row, 7, out));
	KT1_CHECK(out.condition == Condition::BeyondMap && out.unitClass == UnitClass::NotAir);
	KT1_CHECK(out.callIn == CallIn::Command && out.action == Action::Refuse && out.commands == 0 && out.internalToo);
	KT1_CHECK_EQ(out.source, 7);
	row.condition = "nothing";
	KT1_CHECK(!Compile(row, 8, out));
	KT1_CHECK_EQ(out.source, 7);
}

KT1_CASE("ORDR-CLASS", "unit classes are the replaced gadgets' def filters") {
	DefFacts tank = Tank();
	KT1_CHECK(InClass(tank, UnitClass::Ground) && InClass(tank, UnitClass::GroundNotCommand));
	KT1_CHECK(InClass(tank, UnitClass::NotAir) && !InClass(tank, UnitClass::IndirectGround));
	tank.Set(DefFlag::CommandUnit, true);
	KT1_CHECK(InClass(tank, UnitClass::Ground) && !InClass(tank, UnitClass::GroundNotCommand));
	tank.speed = 0;
	KT1_CHECK(!InClass(tank, UnitClass::Ground) && InClass(tank, UnitClass::Mobile));
	DefFacts plane = Tank();
	plane.Set(DefFlag::CanFly, true);
	plane.Set(DefFlag::IndirectFire, true);
	KT1_CHECK(!InClass(plane, UnitClass::Ground) && !InClass(plane, UnitClass::IndirectGround));
	plane.landShortGap = 32;
	KT1_CHECK(!InClass(plane, UnitClass::GapTransport));
	plane.cargoShortGap = 16;
	KT1_CHECK(InClass(plane, UnitClass::GapTransport));
	DefFacts factory = Tank();
	factory.Set(DefFlag::Factory, true);
	KT1_CHECK(!InClass(factory, UnitClass::MobileNonFactory));
}

KT1_CASE("ORDR-MATCH", "a row is asked for its commands and class; internal orders pass unless it says otherwise") {
	CompiledRow row;
	row.commands = MaskOf(CommandKind::Move) | MaskOf(CommandKind::Patrol);
	row.unitClass = UnitClass::NotAir;
	const DefFacts tank = Tank();
	KT1_CHECK(Matches(row, CommandKind::Move, tank, false));
	KT1_CHECK(!Matches(row, CommandKind::Attack, tank, false));
	KT1_CHECK(!Matches(row, CommandKind::Move, tank, true));
	row.internalToo = true;
	KT1_CHECK(Matches(row, CommandKind::Move, tank, true));
	row.commands = 0;
	KT1_CHECK(Matches(row, CommandKind::Other, tank, false));
}

KT1_CASE("ORDR-MAP", "a point is beyond the map only past an edge") {
	KT1_CHECK(!BeyondMap(0, 0, 100, 200));
	KT1_CHECK(!BeyondMap(100, 200, 100, 200));
	KT1_CHECK(BeyondMap(-1, 5, 100, 200) && BeyondMap(5, 201, 100, 200) && BeyondMap(101, 5, 100, 200));
}

KT1_CASE("ORDR-SIGHT", "unseen enemies, known targets and their range") {
	KT1_CHECK(!EnemyUnseen(true, false, 0));
	KT1_CHECK(EnemyUnseen(false, false, 0));
	KT1_CHECK(!EnemyUnseen(false, true, 1) && !EnemyUnseen(false, true, 2));
	KT1_CHECK(EnemyUnseen(false, true, 4) && EnemyUnseen(false, true, 8));
	KT1_CHECK(KnownNotSeen(2) && KnownNotSeen(4) && !KnownNotSeen(3) && !KnownNotSeen(0) && !KnownNotSeen(8));
	KT1_CHECK(KnownTargetInRange(10, 100) && !KnownTargetInRange(10, 101) && !KnownTargetInRange(0, 0));
}

KT1_CASE("ORDR-TARGETS", "what may not be reclaimed, and the engine attack-move rewrite") {
	DefFacts d = Tank();
	d.Set(DefFlag::Reclaimable, true);
	KT1_CHECK(!Unreclaimable(1, 2, d) && Unreclaimable(2, 2, d));
	d.Set(DefFlag::Reclaimable, false);
	KT1_CHECK(Unreclaimable(1, 2, d));
	KT1_CHECK(AttackMoveRewrite(d) == CommandKind::Move);
	d.reach = 100;
	KT1_CHECK(AttackMoveRewrite(d) == CommandKind::AMove);
}

KT1_CASE("ORDR-DEFACCEPTS", "a def's accepted set refuses only the seven kinds it names") {
	DefFacts d = Tank();
	KT1_CHECK(DefAccepts(d, CommandKind::Attack));
	d.Set(DefFlag::HasSpecAccepts, true);
	d.specAccepts = BitOf(AcceptWord::Patrol);
	KT1_CHECK(DefAccepts(d, CommandKind::Patrol));
	KT1_CHECK(!DefAccepts(d, CommandKind::Attack) && !DefAccepts(d, CommandKind::FireState));
	KT1_CHECK(DefAccepts(d, CommandKind::Move) && DefAccepts(d, CommandKind::AMove) && DefAccepts(d, CommandKind::Stop));
}

KT1_CASE("ORDR-FIRE", "fire states cycle, take a floored parameter and follow an engine order") {
	KT1_CHECK(NextFireState(FireState::Hold, false, 0) == FireState::Free);
	KT1_CHECK(NextFireState(FireState::Ground, false, 0) == FireState::Hold);
	KT1_CHECK(NextFireState(FireState::Hold, true, 2.7f) == FireState::Ground);
	KT1_CHECK(NextFireState(FireState::Hold, true, 4) == FireState::Free);
	KT1_CHECK(NextFireState(FireState::Hold, true, -1) == FireState::Ground);
	KT1_CHECK(FireFromEngine(0, 0) == FireState::Hold && FireFromEngine(1, 0) == FireState::Free);
	KT1_CHECK(DiveWanted(true, 1, true) && !DiveWanted(true, 0, false));
	KT1_CHECK(DiveWanted(false, 0, false) && !DiveWanted(false, 0, true));
}

KT1_CASE("ORDR-SHORT", "short moves and queued moves near each other") {
	KT1_CHECK(ShortMove(3, 0, 4) && !ShortMove(4, 0, 4) && !ShortMove(0, 0, 0));
	const std::array<OrderView, 2> queue = {MoveTo(100, 100), MoveTo(200, 200, 1)};
	KT1_CHECK(NearQueuedMove(queue, 110, 100, 17));
	KT1_CHECK(!NearQueuedMove(queue, 117, 100, 17));
	KT1_CHECK(!NearQueuedMove(queue, 200, 200, 17));
}

KT1_CASE("ORDR-REST", "a goal goes to its cell's centre, short of it along the approach") {
	float x = 0, z = 0;
	RestPoint(13, 27, 5, 25, 10, 2, x, z);
	KT1_CHECK_EQ(x, 13.0f);
	KT1_CHECK_EQ(z, 25.0f);
	RestPoint(13, 27, 15, 24, 10, 2, x, z);   // nearer the centre than the rest distance: the centre itself
	KT1_CHECK_EQ(x, 15.0f);
	KT1_CHECK_EQ(z, 25.0f);
}

KT1_CASE("ORDR-UNLOAD", "set-down short of a held spot") {
	UnloadPoints p;
	KT1_CHECK(PlanUnloadPoints(0, 0, 100, 0, 20, 10, 0.000001f, p));
	KT1_CHECK_EQ(p.landX, 80.0f);
	KT1_CHECK_EQ(p.cargoX, 90.0f);
	KT1_CHECK(!PlanUnloadPoints(100, 0, 100, 0, 20, 10, 0.000001f, p));
	KT1_CHECK(HoldsSpot(0, 0, 5, 9, 0, 5) && !HoldsSpot(0, 0, 5, 10, 0, 5));
}

KT1_CASE("ORDR-AREA", "area reclaim takes the nearest first, ties by feature id") {
	std::array<AreaFeature, 3> f = {AreaFeature{9, 4}, AreaFeature{3, 4}, AreaFeature{5, 1}};
	SortAreaFeatures(f);
	KT1_CHECK_EQ(f[0].feature, 5);
	KT1_CHECK_EQ(f[1].feature, 3);
	KT1_CHECK_EQ(f[2].feature, 9);
}

KT1_RATE_CASE("ORDR-STILL", "reclaim starts a latency after the builder stands still; moving or a gap restarts it") {
	Rate::SetStepsPerSecond(testCase.StepsPerSecond());
	const auto latency = Rate::Steps(0.1);
	ReclaimStill record;
	for (int step = 0; step < latency; ++step) KT1_CHECK(!AskReclaim(record, true, step, latency));
	KT1_CHECK(AskReclaim(record, true, latency, latency));
	KT1_CHECK(!AskReclaim(record, false, latency + 1, latency));
	ReclaimStill gap;
	AskReclaim(gap, true, 0, latency);
	KT1_CHECK(!AskReclaim(gap, true, latency + 2, latency));
	KT1_CHECK(Still(0.05f, 0, 0.05f) && !Still(0.05f, 0.01f, 0.05f));
}

KT1_CASE("ORDR-TRANSFER", "a transferred factory keeps only the item in progress") {
	KT1_CHECK_EQ(TransferKeep(true, 5), 1);
	KT1_CHECK_EQ(TransferKeep(false, 5), 0);
	KT1_CHECK_EQ(TransferKeep(true, 0), 0);
}

KT1_CASE("ORDR-STATE", "guard state follows removals, and the re-issue guard is one bit per row") {
	State state;
	state.Init(64);
	state.SetReissuing(10, 0, true);
	state.SetReissuing(10, 63, true);
	state.SetReissuing(20, 5, true);
	state.SetReissuing(20, 64, true);   // past the last row: ignored
	KT1_CHECK(state.Reissuing(10, 0) && state.Reissuing(10, 63) && !state.Reissuing(10, 5));
	KT1_CHECK(!state.Reissuing(20, 64) && !state.Reissuing(30, 0));
	state.landers.Insert(10);
	state.transfers.Insert(10);
	state.Forget(10);
	KT1_CHECK(state.Reissuing(20, 5) && !state.Reissuing(10, 0));
	KT1_CHECK(state.landers.Empty() && state.transfers.Empty());
	state.SetReissuing(20, 5, false);
	KT1_CHECK(!state.Reissuing(20, 5));
	KT1_CHECK(state.Track(100) == DenseIndex::NONE);
}
