/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/HullTurn/HullTurnCore.h, engine-free: the cases of cmb_hullturn.lua's Update (REQ MOV-005 v3 R6, v4 R7).

#include "../KT1Test.h"

#include <cmath>

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/HullTurn/HullTurnCore.h"

using namespace KT1::Core::HullTurn;

namespace {
	float A2(float y, float x) { return std::atan2(y, x); }
	float S(float a) { return std::sin(a); }
	float C(float a) { return std::cos(a); }
	const Trig TRIG = {A2, S, C, static_cast<float>(3.14159265358979323846)};

	Params P() {
		Params p;
		p.rest = 1.5f / 30;   // the config's 1.5 elmos per second at 30 steps
		p.done = 1e-3f;
		p.axis = 1e-4f;
		return p;
	}

	// one fixed weapon (arc 0, tolerance 2 degrees), range 300, turn step 0.05 rad
	DefSpec Fixed(bool hover = false) {
		DefSpec s;
		s.step = 0.05f;
		s.hover = hover;
		s.weapons = 1;
		s.w[0] = {0.0f, 2 * TRIG.pi / 180};
		return s;
	}

	// at the origin facing +z (a quarter-step off the axis), idle, fire at will, its weapon targeting (tx, tz)
	View Idle(float tx, float tz) {
		View v;
		v.fireState = 2;
		v.dx = std::sin(0.3f); v.dz = std::cos(0.3f);
		v.weapons = 1;
		v.range[0] = 300;
		v.wkind = 1; v.wx = tx; v.wz = tz;
		return v;
	}
}

KT1_CASE("HULL-R6-TURN", "an idle unit with its target beside it turns towards it by one step per visit") {
	UnitState r; Shared sh;
	const Out o = Update(r, Fixed(), Idle(100, 0), P(), TRIG, sh);
	KT1_CHECK(o.face);
	KT1_CHECK(r.turning);
	KT1_CHECK(std::fabs(o.faceA - (0.3f + 0.05f)) < 1e-6f);   // bearing +x is +90 degrees: turn positive
	KT1_CHECK_EQ(o.freeWrites, 0);
}

KT1_CASE("HULL-R6-COVERED", "a target already inside the arc causes no turn") {
	UnitState r; Shared sh;
	const Out o = Update(r, Fixed(), Idle(std::sin(0.3f) * 100, std::cos(0.3f) * 100), P(), TRIG, sh);
	KT1_CHECK(!o.face);
	KT1_CHECK(!r.turning);
}

KT1_CASE("HULL-R6-OUT-OF-RANGE", "a target no weapon reaches causes no turn") {
	UnitState r; Shared sh;
	KT1_CHECK(!Update(r, Fixed(), Idle(1000, 0), P(), TRIG, sh).face);
}

KT1_CASE("HULL-ORD-002-HOLD", "hold fire never turns") {
	UnitState r; Shared sh;
	View v = Idle(100, 0);
	v.fireState = 0;
	KT1_CHECK(!Update(r, Fixed(), v, P(), TRIG, sh).face);
}

KT1_CASE("HULL-R6-MOVING", "a ground unit that moves (above the rest speed) or has a move order does not turn") {
	UnitState r; Shared sh;
	View v = Idle(100, 0);
	v.speed = 0.06f;
	KT1_CHECK(!Update(r, Fixed(), v, P(), TRIG, sh).face);
	v.speed = 0;
	v.cmd = Cmd::Moving;
	KT1_CHECK(!Update(r, Fixed(), v, P(), TRIG, sh).face);
}

KT1_CASE("HULL-R6-ATTACK-GROUND", "an attack order on a ground point is the target, ahead of the weapon's own") {
	UnitState r; Shared sh;
	View v = Idle(std::sin(0.3f) * 100, std::cos(0.3f) * 100);   // the weapon's own target is covered
	v.cmd = Cmd::Attack; v.nparams = 3; v.p1 = -100; v.p3 = 0;    // the order's point is to the left
	const Out o = Update(r, Fixed(), v, P(), TRIG, sh);
	KT1_CHECK(o.face);
	KT1_CHECK(std::fabs(o.faceA - (0.3f - 0.05f)) < 1e-6f);
}

KT1_CASE("HULL-R6-LAST-STEP", "the last step is only what is left to the arc edge, and a turning unit carries on to it") {
	UnitState r; Shared sh;
	const float b = 0.02f;   // the target 0.02 rad off the hull, the arc 0 and the tolerance 2 degrees: covered
	View v = Idle(std::sin(0.3f + b) * 100, std::cos(0.3f + b) * 100);
	KT1_CHECK(!Update(r, Fixed(), v, P(), TRIG, sh).face);
	r.turning = true;        // but a unit that turned last step goes on to within `done`
	const Out o = Update(r, Fixed(), v, P(), TRIG, sh);
	KT1_CHECK(o.face);
	KT1_CHECK(std::fabs(o.faceA - (0.3f + b)) < 1e-5f);
}

KT1_CASE("HULL-AXIS", "a heading on an exact axis is kept `axis` short of it, against the turn") {
	Out o;
	Detail::Face(TRIG.pi / 2, 1, P(), TRIG, o);
	KT1_CHECK(o.faceA < TRIG.pi / 2);
	KT1_CHECK(std::fabs(o.faceA - (TRIG.pi / 2 - 1e-4f)) < 1e-6f);
	Detail::Face(0, -1, P(), TRIG, o);
	KT1_CHECK(std::fabs(o.faceA - 1e-4f) < 1e-7f);
}

KT1_CASE("HULL-WRAP", "bearings wrap into [-pi, pi)") {
	KT1_CHECK(std::fabs(Wrap(3 * TRIG.pi / 2, TRIG) + TRIG.pi / 2) < 1e-5f);
	KT1_CHECK(std::fabs(Wrap(-3 * TRIG.pi / 2, TRIG) - TRIG.pi / 2) < 1e-5f);
	KT1_CHECK(std::fabs(Wrap(0.25f, TRIG) - 0.25f) < 1e-6f);
}

KT1_CASE("HULL-R7-TRACK", "a hover unit under a move order takes the hull and turns towards a target out of its arcs") {
	UnitState r; Shared sh;
	View v = Idle(100, 0);
	v.cmd = Cmd::Moving; v.nparams = 3; v.p1 = 0; v.p3 = 500;
	v.speed = 1; v.vx = 0; v.vz = 1;
	const Out o = Update(r, Fixed(true), v, P(), TRIG, sh);
	KT1_CHECK_EQ(o.freeWrites, 1);
	KT1_CHECK(o.free[0]);
	KT1_CHECK(r.free);
	KT1_CHECK(o.face);
	KT1_CHECK(r.turning);
}

KT1_CASE("HULL-R7-RETURN", "with nothing to track the hull turns back to the travel direction, then is handed back") {
	UnitState r; Shared sh;
	r.free = true;
	View v = Idle(0, 0);
	v.wkind = 0;
	v.cmd = Cmd::Moving; v.nparams = 3; v.p1 = 0; v.p3 = 500;
	v.speed = 1; v.vx = 0; v.vz = 1;   // travelling along +z; the hull is 0.3 rad off
	Out o = Update(r, Fixed(true), v, P(), TRIG, sh);
	KT1_CHECK(o.face);
	KT1_CHECK(std::fabs(o.faceA - (0.3f - 0.05f)) < 1e-6f);
	KT1_CHECK(r.free);
	v.dx = std::sin(0.01f); v.dz = std::cos(0.01f);   // within one step: face the route and hand back
	o = Update(r, Fixed(true), v, P(), TRIG, sh);
	KT1_CHECK(o.face);
	KT1_CHECK_EQ(o.freeWrites, 1);
	KT1_CHECK(!o.free[0]);
	KT1_CHECK(!r.free);
}

KT1_CASE("HULL-R7-OTHER-ORDER", "any other order hands the hull back at once") {
	UnitState r; Shared sh;
	r.free = true;
	View v = Idle(100, 0);
	v.cmd = Cmd::Other;
	const Out o = Update(r, Fixed(true), v, P(), TRIG, sh);
	KT1_CHECK(!o.face);
	KT1_CHECK_EQ(o.freeWrites, 1);
	KT1_CHECK(!r.free);
}

KT1_CASE("HULL-SWITCH-LATCH", "a move type that is not a ground one stops every heading-free write, as the Lua's pcall did") {
	UnitState r; Shared sh;
	View v = Idle(100, 0);
	v.cmd = Cmd::Moving;
	v.groundMoveType = false;
	Update(r, Fixed(true), v, P(), TRIG, sh);
	KT1_CHECK(!sh.switchOK);
	KT1_CHECK(!r.free);
}

KT1_RATE_CASE("HULL-DUE", "a resting unit is visited every 0.1 s, phased by id; a turning one every step") {
	const int sps = testCase.StepsPerSecond();
	KT1::Core::Rate::SetStepsPerSecond(sps);
	const int32_t every = KT1::Core::Rate::Every(0.1);
	KT1_CHECK_EQ(every, sps == 30 ? 3 : 1);
	UnitState r;
	int visits = 0;
	for (int f = 0; f < sps; ++f) visits += Due(r, f, 7, every) ? 1 : 0;
	KT1_CHECK_EQ(visits, sps / every);
	r.turning = true;
	KT1_CHECK(Due(r, 1, 7, every));
}
