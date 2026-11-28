/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 sound cues (LUA-FREE D10): the rules snd_events held, at 30 and 10 sim steps per second.

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/Cues/CueState.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::Cues;
using KT1::Core::Rate;
using KT1::Core::TeamRelation;

namespace {

struct Fixture {
	CueConfig config;
	CueState state;
	CueParams params;
	explicit Fixture(int32_t sps) {
		Rate::SetStepsPerSecond(sps);
		state.Init(64, 8, config);
		params = MakeParams(config);
		state.AddUnit(5, {true, true, false});
	}
	int Admitted(int32_t steps, int32_t asked) {
		int n = 0;
		for (int32_t s = 0; s < steps; ++s) {
			BeginStep(state, params, s);
			for (int32_t k = 0; k < asked; ++k) n += Admit(state) ? 1 : 0;
		}
		return n;
	}
};

FuelFacts Fuel(float fuel, bool pad = false, float health = 100.0f) { return {fuel, 100.0f, -1.0f, pad, health, 100.0f}; }

} // namespace

KT1_CASE("SND-PARAMS", "seconds become steps and per-step shares of the rate") {
	const Fixture a(30);
	KT1_CHECK_EQ(a.params.moveCheckSteps, 10);
	KT1_CHECK_EQ(a.params.beamGapSteps, 6);
	KT1_CHECK_EQ(a.params.captureStallSteps, 30);
	KT1_CHECK_EQ(a.params.shieldHitGapSteps, 8);
	const Fixture b(10);
	KT1_CHECK_EQ(b.params.moveCheckSteps, 3);
	KT1_CHECK_EQ(b.params.beamGapSteps, 2);
	KT1_CHECK_EQ(b.params.captureStallSteps, 10);
}

KT1_CASE("SND-LIMIT-30", "480 a second is 16 a step at 30; the rest is counted as cut; unused ones are not saved") {
	Fixture f(30);
	KT1_CHECK_EQ(f.Admitted(30, 20), 480);
	KT1_CHECK_EQ(f.state.cut, 120u);
	BeginStep(f.state, f.params, 30);
	BeginStep(f.state, f.params, 31);
	int n = 0;
	for (int k = 0; k < 40; ++k) n += Admit(f.state) ? 1 : 0;
	KT1_CHECK_EQ(n, 16);
}

KT1_CASE("SND-LIMIT-10", "the same 480 a second at 10 steps; a rate under one a step still passes") {
	Fixture f(10);
	KT1_CHECK_EQ(f.Admitted(10, 100), 480);
	f.config.max_per_s = 5.0f;
	f.state.Init(64, 8, f.config);
	f.params = MakeParams(f.config);
	KT1_CHECK_EQ(f.Admitted(20, 3), 10);
}

KT1_CASE("SND-ROWS", "a row matches its event and filters; gap and per-second limits") {
	Fixture f(10);
	CueRow row = PlainRow("Explosion", "impact");
	KT1_CHECK(RowMatches(row, "Explosion", "tank", "gun", TeamRelation::Enemy));
	KT1_CHECK(!RowMatches(row, "UnitDestroyed", "tank", "gun", TeamRelation::Enemy));
	row.map.weapon = "gun";
	row.map.relation = "allied";
	KT1_CHECK(RowMatches(row, "Explosion", "", "gun", TeamRelation::Own));
	KT1_CHECK(!RowMatches(row, "Explosion", "", "gun", TeamRelation::Enemy));
	KT1_CHECK(!RowMatches(row, "Explosion", "", "laser", TeamRelation::Own));
	f.config.rows[0].max_per_s = 2.0f;
	f.config.rows[1].min_gap_s = 0.5f;
	BeginStep(f.state, f.params, 0);
	KT1_CHECK(RowAdmit(f.state, f.config, 0));
	KT1_CHECK(RowAdmit(f.state, f.config, 0));
	KT1_CHECK(!RowAdmit(f.state, f.config, 0));
	KT1_CHECK(RowAdmit(f.state, f.config, 1));
	BeginStep(f.state, f.params, 4);
	KT1_CHECK(!RowAdmit(f.state, f.config, 1));
	BeginStep(f.state, f.params, 5);
	KT1_CHECK(RowAdmit(f.state, f.config, 1));
	BeginStep(f.state, f.params, 10);
	KT1_CHECK(RowAdmit(f.state, f.config, 0));
	KT1_CHECK(!RowAdmit(f.state, f.config, 99));
}

KT1_CASE("SND-IDS-LOG", "config rows name the first cue ids; the log keeps step, unit, cue and place") {
	Fixture f(30);
	KT1_CHECK_EQ(CueId(f.state, "fire"), 0);
	KT1_CHECK_EQ(CueId(f.state, "move_stop"), 3);
	KT1_CHECK_EQ(CueId(f.state, "dock"), 4);
	KT1_CHECK_EQ(CueId(f.state, "dock"), 4);
	BeginStep(f.state, f.params, 7);
	Log(f.state, 5, 4, {1.0f, 2.0f, 3.0f});
	KT1_CHECK_EQ(f.state.log.size(), size_t(1));
	KT1_CHECK_EQ(f.state.log[0].step, 7);
	KT1_CHECK_EQ(f.state.log[0].unit, 5);
}

KT1_CASE("SND-BEAM", "one impact cue of a beam weapon per gap") {
	Fixture f(30);
	BeginStep(f.state, f.params, 0);
	KT1_CHECK(BeamAdmit(f.state, f.params, 2));
	BeginStep(f.state, f.params, 5);
	KT1_CHECK(!BeamAdmit(f.state, f.params, 2));
	KT1_CHECK(BeamAdmit(f.state, f.params, 3));
	BeginStep(f.state, f.params, 6);
	KT1_CHECK(BeamAdmit(f.state, f.params, 2));
}

KT1_CASE("SND-MOVE-LOOP", "each unit once per share; start and stop only on a change; a removed unit ends its loop") {
	Fixture f(30);
	int due = 0;
	for (int32_t s = 0; s < 10; ++s) due += MoveDue(f.params, 5, s) ? 1 : 0;
	KT1_CHECK_EQ(due, 1);
	KT1_CHECK(!MoveChanged(f.state, 5, false));
	KT1_CHECK(MoveChanged(f.state, 5, true));
	KT1_CHECK(!MoveChanged(f.state, 5, true));
	KT1_CHECK(MoveChanged(f.state, 5, false));
	KT1_CHECK(!MoveChanged(f.state, 6, true));            // unknown unit
	f.state.AddUnit(9, {false, false, true});
	KT1_CHECK(LoopChanged(f.state, 9, true));
	KT1_CHECK(MoveChanged(f.state, 5, true));
	KT1_CHECK(!f.state.RemoveUnit(5));
	KT1_CHECK(!LoopChanged(f.state, 9, true));            // moved into the hole, state kept
	KT1_CHECK(f.state.RemoveUnit(9));
	f.state.AddUnit(9, {false, false, true});
	KT1_CHECK(LoopChanged(f.state, 9, true));             // the id again starts blank
}

KT1_CASE("SND-FUEL", "low, empty, refuel and repair once per episode") {
	Fixture f(30);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(50)), 0);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(18)), FUEL_LOW);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(17)), 0);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(0)), FUEL_EMPTY);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(5, true, 50)), FUEL_REFUEL);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(30, true, 60)), FUEL_REPAIR);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(60, true, 70)), 0);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(100, true, 100)), 0);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(10)), FUEL_LOW);
	FuelForget(f.state, 5);
	KT1_CHECK_EQ(FuelStep(f.state, f.params, 5, Fuel(10)), FUEL_LOW);   // seen anew: low and not rising
}

KT1_CASE("SND-CAPTURE", "start at the first progress, one stall after a second without growth or when the work is lost") {
	Fixture f(10);
	BeginStep(f.state, f.params, 1);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.0f) == CaptureCue::None);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.1f) == CaptureCue::Start);
	BeginStep(f.state, f.params, 10);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.1f) == CaptureCue::None);
	BeginStep(f.state, f.params, 11);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.1f) == CaptureCue::Stall);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.1f) == CaptureCue::None);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.2f) == CaptureCue::None);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.0f) == CaptureCue::Stall);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.3f) == CaptureCue::Start);
	CaptureEnd(f.state, 5);
	KT1_CHECK(CaptureStep(f.state, f.params, 5, 9, 0.0f) == CaptureCue::None);
}

KT1_CASE("SND-LAYER", "dive and surface on a change; the first sight plays nothing") {
	Fixture f(30);
	KT1_CHECK(LayerStep(f.state, 5, true) == LayerCue::None);
	KT1_CHECK(LayerStep(f.state, 5, true) == LayerCue::None);
	KT1_CHECK(LayerStep(f.state, 5, false) == LayerCue::Surface);
	KT1_CHECK(LayerStep(f.state, 5, true) == LayerCue::Dive);
}

KT1_CASE("SOUND-SHIELD", "hit sounds at most once per gap; state changes: up, break, fall; gone forgets") {
	Fixture f(30);
	BeginStep(f.state, f.params, 0);
	KT1_CHECK(ShieldHitAdmit(f.state, f.params, 5));
	BeginStep(f.state, f.params, 7);
	KT1_CHECK(!ShieldHitAdmit(f.state, f.params, 5));
	BeginStep(f.state, f.params, 8);
	KT1_CHECK(ShieldHitAdmit(f.state, f.params, 5));
	KT1_CHECK(ShieldStateStep(f.state, 5, 1, false) == ShieldCue::None);
	KT1_CHECK(ShieldStateStep(f.state, 5, 0, true) == ShieldCue::Break);
	KT1_CHECK(ShieldStateStep(f.state, 5, 0, true) == ShieldCue::None);
	KT1_CHECK(ShieldStateStep(f.state, 5, 1, false) == ShieldCue::Up);
	KT1_CHECK(ShieldStateStep(f.state, 5, 0, false) == ShieldCue::Fall);
	KT1_CHECK(ShieldStateStep(f.state, 5, -1, false) == ShieldCue::None);
	KT1_CHECK(ShieldStateStep(f.state, 5, 0, false) == ShieldCue::None);
}

KT1_CASE("SOUND-ZOOM", "a cue is held back above its cut; cut 0 never holds") {
	KT1_CHECK(!ZoomHeld(5000.0f, 0.0f));
	KT1_CHECK(!ZoomHeld(1000.0f, 1000.0f));
	KT1_CHECK(ZoomHeld(1000.5f, 1000.0f));
}
